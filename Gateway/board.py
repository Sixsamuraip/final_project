""" Defines the BaseBoard class for RA-02 LoRa module.

Based on the board configuration code from
https://github.com/rpsreal/pySX127x/blob/master/SX127x/board_config.py
"""

import time
import spidev
import RPi.GPIO as GPIO

###########################################################
class MySPI(spidev.SpiDev):

    def __init__(self, bus, cs, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.cs_pin = cs
        if cs == 8:  # hardware cs pin CE0
            self.open(bus, 0)
            self.no_cs = False
        elif cs == 7:  # hardware cs pin CE1
            self.open(bus, 1)
            self.no_cs = False
        else:
            self.open(bus,0)
            self.no_cs = True
            GPIO.setup(self.cs_pin, GPIO.OUT)
            GPIO.output(self.cs_pin, GPIO.HIGH)
        self.max_speed_hz = 100000  # SX127x can go up to 10MHz, pick half that to be safe
            
    def xfer(self, data):
        if self.cs_pin:
            GPIO.output(self.cs_pin, GPIO.LOW)
        #print('-'*80)
        #print('MOSI:', ' '.join(f'{x:02X}' for x in data))
        ret = super().xfer(data)
        #print('MISO:', ' '.join(f'{x:02X}' for x in ret))
        if self.cs_pin:
            GPIO.output(self.cs_pin, GPIO.HIGH)
        return ret

###########################################################
class BaseBoard:

    # The spi object is kept here
    spi = None

    # tell pySX127x here whether the attached RF module uses low-band (RF*_LF pins) or high-band (RF*_HF pins).
    # low band (called band 1&2) are 137-175 and 410-525
    # high band (called band 3) is 862-1020
    low_band = True

    @classmethod
    def setup(cls):
        """ Configure the Raspberry GPIOs
        :rtype : None
        """
        GPIO.setmode(GPIO.BCM)
        # LED
        if cls.LED:
            GPIO.setup(cls.LED, GPIO.OUT)
            GPIO.output(cls.LED, 1)
        GPIO.setup(cls.RST, GPIO.OUT)
        GPIO.output(cls.RST, 1)
        # switch
        #GPIO.setup(cls.SWITCH, GPIO.IN, pull_up_down=GPIO.PUD_DOWN) 
        # DIOx
        for gpio_pin in [cls.DIO0, cls.DIO1]:
            GPIO.setup(gpio_pin, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
        # blink 2 times to signal the board is set up
        cls.blink(.1, 2)

    @classmethod
    def teardown(cls):
        """ Cleanup GPIO and SpiDev """
        GPIO.cleanup()
        cls.spi.close()

    @classmethod
    def SpiDev(cls):
        """ Init and return the SpiDev object
        :return: SpiDev object
        :param spi_bus: The RPi SPI bus to use: 0 or 1
        :param spi_cs: The RPi SPI chip select to use: 0 or 1
        :rtype: SpiDev
        """
        spi_bus=cls.SPI_BUS
        spi_cs=cls.SPI_CS
        #cls.spi = spidev.SpiDev()
        cls.spi = MySPI(spi_bus,spi_cs)
        return cls.spi

    @classmethod
    def add_event_detect(cls, dio_number, callback):
        """ Wraps around the GPIO.add_event_detect function
        :param dio_number: DIO pin 0...5
        :param callback: The function to call when the DIO triggers an IRQ.
        :return: None
        """
        GPIO.add_event_detect(dio_number, GPIO.RISING, callback=callback)

    @classmethod
    def add_events(cls, cb_dio0, cb_dio1, cb_dio2, cb_dio3, cb_dio4, cb_dio5, switch_cb=None):
        cls.add_event_detect(cls.DIO0, callback=cb_dio0)
        cls.add_event_detect(cls.DIO1, callback=cb_dio1)
        # the modtronix inAir9B does not expose DIO4 and DIO5
        if switch_cb is not None:
            GPIO.add_event_detect(cls.SWITCH, GPIO.RISING, callback=switch_cb, bouncetime=300)

    @classmethod
    def led_on(cls):
        """ Switch the proto shields LED
        :param value: 0/1 for off/on. Default is 1.
        :return: value
        :rtype : int
        """
        if cls.LED:
            GPIO.output(cls.LED, 0)
        return 1

    @classmethod
    def led_off(cls):
        """ Switch LED off
        :return: 0
        """
        if cls.LED:
            GPIO.output(cls.LED, 1)
        return 0
    
    @classmethod
    def reset(cls):
        """ manual reset
        :return: 0
        """
        GPIO.output(cls.RST, 0)
        time.sleep(.01)
        GPIO.output(cls.RST, 1)
        time.sleep(.01)
        return 0

    @classmethod
    def blink(cls, time_sec, n_blink):
        if n_blink == 0:
            return
        cls.led_on()
        for i in range(n_blink):
            time.sleep(time_sec)
            cls.led_off()
            time.sleep(time_sec)
            cls.led_on()
        cls.led_off()