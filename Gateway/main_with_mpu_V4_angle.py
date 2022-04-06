import sys
import time
from datetime import datetime, timedelta
import asyncio
import ctypes
import RPi.GPIO as GPIO
from SX127x.constants import *
from lora import LoRa
from board import BaseBoard
from util import *
#from util import TimeStamp,StrToAscii,printT,writeCSV,upload_to_mongo,printT
import pymongo
from pymongo import MongoClient

SF = 12
CH0_FREQ = 434.0
CH1_FREQ = 444.0

PKT_TYPE_ADVERTISE = 0x01
PKT_TYPE_REPORT    = 0x02
PKT_TYPE_ACK       = 0x03
PKT_TYPE_BOOT      = 0x04

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <log>")
    exit(1)

if sys.argv[1] == '-':
    LOG_FILE = None
else:
    LOG_FILE = open(sys.argv[1],'a')

# Setup

GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)
loop = asyncio.get_event_loop()

class Board0(BaseBoard):
    # Note that the BCM numbering for the GPIOs is used.
    DIO0    = 24
    DIO1    = 6
    RST     = 17
    LED     = 4
    SPI_BUS = 0
    SPI_CS  = 8


class Board1(BaseBoard):
    # Note that the BCM numbering for the GPIOs is used.
    DIO0    = 12
    DIO1    = 19
    RST     = 27
    LED     = None
    SPI_BUS = 0
    SPI_CS  = 20

# Setup Channel

configs = [
        {'name':"CH0",'board':Board0, 'bw':BW.BW125, 'freq':CH0_FREQ, 'cr':CODING_RATE.CR4_8, 'sf':SF},
        {'name':"CH1",'board':Board1, 'bw':BW.BW125, 'freq':CH1_FREQ, 'cr':CODING_RATE.CR4_8, 'sf':SF}
]

for config in configs:
    board = config['board']
    GPIO.setup(board.SPI_CS, GPIO.OUT)
    GPIO.output(board.SPI_CS, GPIO.HIGH)
    board.setup()
    board.reset()

###########################################################
class BaseStruct(ctypes.Structure):

    _pack_ = 1

    @classmethod
    def size(cls):
        return ctypes.sizeof(cls)

    def unpack(self, blob):
        if not isinstance(blob,bytes):
            raise Exception('Byte array expected for blob')
        if ctypes.sizeof(self) != len(blob):
            raise Exception('Size mismatched')
        ctypes.memmove(ctypes.addressof(self), blob, ctypes.sizeof(self))

###########################################################
class StructConfig(BaseStruct):
    _fields_ = [
        ('radio_device_address', ctypes.c_ubyte),
        ('radio_gateway_address', ctypes.c_ubyte),
        ('radio_freq', ctypes.c_float),
        ('radio_tx_power', ctypes.c_ubyte),
        ('collect_interval_day', ctypes.c_ushort),
        ('collect_interval_night', ctypes.c_ushort),
        ('day_start_hour', ctypes.c_ubyte),
        ('day_end_hour', ctypes.c_ubyte),
        ('time_zone', ctypes.c_byte),
        ('advertise_interval', ctypes.c_ushort),
        ('use_ack', ctypes.c_byte),
        ('ack_timeout', ctypes.c_ushort),
        ('long_range', ctypes.c_ubyte),
        ('tx_repeat', ctypes.c_ubyte),
        ('gps_max_wait_for_fix', ctypes.c_ushort),
        ('next_collect_no_fix', ctypes.c_ushort),
        ('total_slots', ctypes.c_ushort),
        ('slot_interval', ctypes.c_ushort),
        ('prog_file_name', ctypes.c_char*10),
    ]

###########################################################
class StructReport(BaseStruct):
    _fields_ = [
        ('year', ctypes.c_ubyte),
        ('month', ctypes.c_ubyte),
        ('day', ctypes.c_ubyte),
        ('hour', ctypes.c_ubyte),
        ('minute', ctypes.c_ubyte),
        ('second', ctypes.c_ubyte),
        ('vbat', ctypes.c_ushort),    # unit of mV
        ('latitude', ctypes.c_long),
        ('longitude', ctypes.c_long), # unit of 1/100000 degrees
        ('quality', ctypes.c_ubyte),
        ('satellites', ctypes.c_ubyte),
        ('temperature', ctypes.c_ushort),
        ('last_heard_from_gw', ctypes.c_ulong),
        ('ax', ctypes.c_short),
        ('ay', ctypes.c_short),
        ('az', ctypes.c_short),
        ('gx', ctypes.c_short),
        ('gy', ctypes.c_short),
        ('gz', ctypes.c_short),
    ]

###########################################################
class StructPktBoot(BaseStruct):
    _fields_ = [
        ('type', ctypes.c_ubyte),
        ('firmware', ctypes.c_ushort),
        ('device_model', ctypes.c_ubyte),
        ('reset_flags', ctypes.c_ubyte),
        ('config', StructConfig),
    ]

###########################################################
class StructPktReport(BaseStruct):
    _fields_ = [
        ('type', ctypes.c_ubyte),
        ('seq', ctypes.c_ubyte),
        ('report', StructReport),
    ]

###########################################################

# Define MyLoRa

class Mylora(LoRa):

    def __init__(self, board, name, verbose=False):

        super(Mylora, self).__init__(board,verbose=verbose)
        self.board = board
        self.set_mode(MODE.SLEEP)
        self.set_dio_mapping([0] * 6)
        self.tx_avail = True
        self.rx_avail = False
        self.name = name
        self.dst = None
        self.ack = 0
        self.ack_mode = 0
        self.n_pkt = 0
        self.ack_repeat = 2
        self.wait_t = 0
        self.time_out = 48000 * 5  #48000 == 1 min

        self.end_device_source = 0
        self.get_boot = 0 # already recive BOOT_PKT or not
    def on_rx_done(self):

        self.board.led_on()
        self.printT(f"Recieve Done Process ...")
        pkt_rssi,rssi = self.get_pkt_rssi_value(), self.get_rssi_value()
        self.clear_irq_flags(RxDone=1)
        payload = self.read_payload(nocheck=True) 
        src = payload[1]
        dst = payload[0]
        pkt = bytes(bytearray(payload[4:]))
        self.board.led_off()
        if pkt[0] == PKT_TYPE_BOOT and len(pkt) == 44 and self.get_boot == 0:
            self.printT("Recieve Boot Pkt")
            self.onPktBoot(src,pkt)
            self.end_device_source = src
            self.get_boot = 1
        
        elif pkt[0] == PKT_TYPE_BOOT and len(pkt) == 44 and self.get_boot == 1:
            self.printT(f'Already recive Boot Pkt!')
        
        elif pkt[0] == PKT_TYPE_REPORT and src == self.end_device_source:
            self.ack_mode = 1 
            self.wait_t = 0
            self.printT("Recieve Report Pkt")
            self.onPktReport(src,pkt)
        elif src == self.end_device_source:
            self.dst = src
            self.printT(f'Unknown packet of size {len(pkt)} bytes received from 0x{src:02X}')
        else: 
            self.dst = src
            self.printT(f'Unknown source received from 0x{src:02X}')

    def on_tx_done(self):

        self.printT(f"Transmit Done")
        self.clear_irq_flags(TxDone=1)
        self.tx_avail = True

    def printT(self,text):

        print(f"{TimeStamp()} [{self.name}] {text}")

    def onPktReport(self,src,pkt):

        report_pkt = StructPktReport()
        report_pkt.unpack(pkt)
        report = report_pkt.report
        self.n_pkt = report_pkt.seq
        rssi = self.get_pkt_rssi_value()
        self.printT('LOG: {} {} 20{:02}-{:02}-{:02} {:02}:{:02}:{:02} {} {} {} {} {} {} {} {} {} {} {} {} {} {}'.format(
            self.n_pkt,
            src,
            report.year,
            report.month,
            report.day,
            report.hour,
            report.minute,
            report.second,
            report.latitude,
            report.longitude,
            report.vbat,
            report.quality,
            report.satellites,
            report.temperature,
            report.last_heard_from_gw,
            report.ax,
            report.ay,
            report.az,
            report.gx,
            report.gy,
            report.gz,
            rssi,
        ))
        try:
            rxtime = datetime.now()
            logtime = datetime(2000+report.year,
                               report.month,
                               report.day,
                               report.hour,
                               report.minute,
                               report.second)
            logtime += timedelta(hours=7)
        except ValueError:
            return
        data = {
            "recorded_time"      : logtime.strftime("%Y-%m-%d %H:%M:%S"),
            "received_time"      : rxtime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "device"             : src,
            "lat"                : report.latitude/1e5,
            "lon"                : report.longitude/1e5,
            "vbat"               : report.vbat/1e3,
            "quality"            : report.quality,
            "satellites"         : report.satellites,
            "temperature"        : report.temperature,
            "last_heard_from_gw" : report.last_heard_from_gw/1000,
            "ax"                 : report.ax,
            "ay"                 : report.ay,
            "az"                 : report.az,
            "angle_x"            : report.gx/100,
            "angle_y"            : report.gy/100,
            "angle_z"            : report.gz/100,
            "rssi"               : rssi,
        }
        # writeCSV([self.n_pkt,
        #     src,
        #     report.year,
        #     report.month,
        #     report.day,
        #     report.hour,
        #     report.minute,
        #     report.second,
        #     report.latitude,
        #     report.longitude,
        #     report.vbat,
        #     report.quality,
        #     report.satellites,
        #     report.temperature,
        #     report.last_heard_from_gw,
        #     report.ax,
        #     report.ay,
        #     report.az,
        #     report.gx,
        #     report.gy,
        #     report.gz,
        #     rssi,],LOG_FILE)
        writeCSV(data,LOG_FILE)
        # result = upload_to_mongo(data)
        # if result == 0:
        #     self.printT("Data upload to server successful")
        # elif result == 1:
        #     self.printT("fail to upload data")
        # elif result == 2:
        #     self.printT("error to connect to server")


        self.rx_avail = True

    def onPktBoot(self,src,pkt):

        boot = StructPktBoot()
        boot.unpack(pkt)
        rssi = self.get_pkt_rssi_value()
        my_addr = boot.config.radio_device_address
        gw_addr = boot.config.radio_gateway_address
        self.dst = my_addr
        if boot.config.use_ack:
            self.ack = 1
            self.ack_mode = 0
        print(f'Device booting reported from {src}, RSSI={rssi}, with parameters:')
        print(f'Firmware version: {boot.firmware}')
        print(f'Device model: 0x{boot.device_model:02x}')
        print(f'Reset flags: 0x{boot.reset_flags:02x}')
        print(f'radio_freq = {boot.config.radio_freq:.2f} MHz')
        print(f'radio_tx_power = {boot.config.radio_tx_power} dBm')
        print(f'radio_device_address = {my_addr} (0x{my_addr:02X})')
        print(f'radio_gateway_address = {gw_addr} (0x{gw_addr:02X})')
        print(f'collect_interval_day = {boot.config.collect_interval_day} sec')
        print(f'collect_interval_night = {boot.config.collect_interval_night} sec')
        print(f'day_start_hour = {boot.config.day_start_hour}')
        print(f'day_end_hour = {boot.config.day_end_hour}')
        print(f'time_zone = {boot.config.time_zone} hours')
        print(f'use_ack = {boot.config.use_ack}')
        print(f'ack_timeout = {boot.config.ack_timeout} sec')
        print(f'long_range = {boot.config.long_range}')
        print(f'tx_repeat = {boot.config.tx_repeat}')
        print(f'gps_max_wait_for_fix = {boot.config.gps_max_wait_for_fix} sec')
        print(f'next_collect_no_fix = {boot.config.next_collect_no_fix} sec')
        print(f'total_slots = {boot.config.total_slots}')
        print(f'slot_interval = {boot.config.slot_interval} sec')
        self.rx_avail = True

    async def toRxMode(self):

        await asyncio.sleep(3)
        self.set_dio_mapping([0] * 6)
        self.reset_ptr_rx()
        self.set_mode(MODE.RXCONT)

    async def sendADV(self):

        self.set_dio_mapping([1,0,0,0,0,0])
        while not self.tx_avail:
            await asyncio.sleep(0.001)
        self.tx_avail = False
        self.write_payload([0xff,0x00,0x00,0x00,0x01,self.n_pkt])
        self.printT("Transmiting ADV Pkt ")
        self.set_mode(MODE.TX)
        await self.toRxMode()
        self.printT("Wait for Report Pkt")

    async def sendACK(self):

        self.set_dio_mapping([1,0,0,0,0,0])
        while not self.tx_avail:
            await asyncio.sleep(0.001)
        self.tx_avail = False
        self.write_payload([0x00+self.dst,0x00,0x00,0x00,0x03,self.n_pkt])
        self.printT(f"Transmiting ACK Pkt for Report {self.n_pkt}")
        self.set_mode(MODE.TX)
        await self.toRxMode()
        self.printT(f"Wait for Report Pkt seq {self.n_pkt + 1}")

    async def start(self): 

        self.printT(f"START f = {self.get_freq()} MHz")
        await self.toRxMode()
        while True:
            if self.ack == 0 and self.ack_mode ==0: # start state -> wait for boot
                self.printT("Wait For Boot Pkt")
                while not self.rx_avail:
                    await asyncio.sleep(0.001)
                self.rx_avail = False
            if self.ack == 1 and self.ack_mode ==0 : # wait for advertise ack
                while not self.rx_avail:
                    self.wait_t += 1
                    await self.sendADV()
                    await asyncio.sleep(10) 
                    if self.wait_t > (self.time_out / 5000):
                        self.wait_t = 0
                        self.ack = 0 
                        self.printT("Wait Report Time Out move to Boot State")
                        self.get_boot = 0
                        break
            if self.ack == 1 and self.ack_mode == 1:    
                while not self.rx_avail:
                    self.wait_t +=1
                    await asyncio.sleep(0.001)
                    if self.wait_t > self.time_out:
                        self.wait_t = 0
                        self.ack = 1
                        self.ack_mode = 0
                        self.printT("Wait Report next seq Time Out move to ADV State")
                        break
                self.rx_avail = False
                await asyncio.sleep(3)
                await self.sendACK()

loras = []

for config in configs:

    lora = Mylora(config['board'],config['name'])
    lora.set_pa_config(pa_select=1, max_power=21, output_power=15)
    lora.set_bw(config['bw'])
    lora.set_freq(config['freq'])
    lora.set_coding_rate(config['cr'])
    lora.set_spreading_factor(config['sf'])
    lora.set_rx_crc(True)
    lora.set_low_data_rate_optim(True)
    assert(lora.get_agc_auto_on() == 1)
    loras.append(lora)

for lora in loras:

    loop.create_task(lora.start())

try:
    loop.run_forever()
except KeyboardInterrupt:
    sys.stdout.flush()
    print("Exit")
    sys.stderr.write("KeyboardInterrupt\n")
