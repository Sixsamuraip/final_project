
#include <Adafruit_SleepyDog.h>
#include "version.h"

#define LOGGING
#define SD_LOADER
#define WATCHDOG_TIMEOUT   10000
#define MAX_TASKS   3
#define GPS_SERIAL  Serial1

#define DEBUG
#include "debug.h"
//#define STORAGE_DEBUG
#include "storage.h"

#include "gps.h"
#include "logger.h"
#include "radio.h"
#include "config.h"
#include "io.h"
#include "time-utils.h"
#include "card-utils.h"
#include "task.h"

//#include <Wire.h>
//#include <I2Cdev.h>
//#include <MPU6050.h>

#include <Wire.h>
#include <I2Cdev.h>
#include "MPU6050_6Axis_MotionApps20_edit.h"

#if defined(SD_LOADER) && defined(ADAFRUIT_FEATHER_M0)
__attribute__ ((section(".sketch_boot")))
unsigned char sduBoot[0xC000] = {
#include "feather_m0.h"
};
#endif

Sd2Card card;
Config config;
Storage<ReportItem> storage(card);
GPSClass GPS(GPS_SERIAL);
Radio radio;
Task* tasks[MAX_TASKS];
uint8_t num_tasks;
volatile bool pps_detected = false;
#ifdef DEBUG
volatile bool wake_now = false;
#endif

#ifdef LOGGING
Logger logger(card);
#endif

//**********************************************

//**************   MPU Set   *******************

bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer


Quaternion q;           // [w, x, y, z]         quaternion container
VectorFloat gravity;    // [x, y, z]            gravity vector
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

int16_t glo_ax, glo_ay, glo_az;
int16_t glo_ag_x, glo_ag_y, glo_ag_z;
MPU6050 mpu(0x69);
void get_accelerometer() {
int16_t ax, ay, az;
int16_t gx, gy, gz;
mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

if (!dmpReady) return;
  // read a packet from FIFO
if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) { // Get the Latest packet 

mpu.dmpGetQuaternion(&q, fifoBuffer);
mpu.dmpGetGravity(&gravity, &q);
mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
}
//valx = map(ax, -17000, 17000, 0, 179);
//valy = map(ay, -17000, 17000, 0, 179);
//valz = map(az, -17000, 17000, 0, 179);
DEBUG_PRINTLN(F("get 3 axis value"));
glo_ax = ax;
glo_ay = ay;
glo_az = az;
glo_ag_x = (ypr[0] * 180/M_PI)*100;
glo_ag_y = (ypr[1] * 180/M_PI)*100;
glo_ag_z = (ypr[2] * 180/M_PI)*100;

DEBUG_PRINTLN(ax);
DEBUG_PRINTLN(ay);
DEBUG_PRINTLN(az);

DEBUG_PRINTLN(glo_ag_x);
DEBUG_PRINTLN(glo_ag_y);
DEBUG_PRINTLN(glo_ag_z);
}

//**********************************************

/**********************************************


/***********************************************
 * 
 */
void init_card() {
  DEBUG_PRINTLN(F("Init MPU6050."));
  Wire.begin();
  pinMode(5, OUTPUT);
  digitalWrite(5,HIGH);
  mpu.initialize();
  Serial.println(mpu.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");

  devStatus = mpu.dmpInitialize();

  mpu.setXGyroOffset(22);
  mpu.setYGyroOffset(-6);
  mpu.setZGyroOffset(-58);
  mpu.setXAccelOffset(28);
  mpu.setYAccelOffset(1254);
  mpu.setZAccelOffset(1364);

  if (devStatus == 0) {
      // Calibration Time: generate offsets and calibrate our MPU6050
      mpu.CalibrateAccel(3);
      mpu.CalibrateGyro(3);
      mpu.PrintActiveOffsets();
      // turn on the DMP, now that it's ready
      Serial.println(F("Enabling DMP..."));
      mpu.setDMPEnabled(true);

//      // enable Arduino interrupt detection
//      Serial.print(F("Enabling interrupt detection (Arduino external interrupt "));
//      Serial.print(digitalPinToInterrupt(INTERRUPT_PIN));
//      Serial.println(F(")..."));
//      attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
      mpuIntStatus = mpu.getIntStatus();

      // set our DMP Ready flag so the main loop() function knows it's okay to use it
      Serial.println(F("DMP ready! Waiting for first interrupt..."));
      dmpReady = true;

      // get expected DMP packet size for later comparison
      packetSize = mpu.dmpGetFIFOPacketSize();
  } else {
      // ERROR!
      // 1 = initial memory load failed
      // 2 = DMP configuration updates failed
      // (if it's going to break, usually the code will be 1)
      Serial.print(F("DMP Initialization failed (code "));
      Serial.print(devStatus);
      Serial.println(F(")"));
    }
  
  // logger still not available
  DEBUG_PRINT("Initializing SD card...");
  if (!card.init(SPI_HALF_SPEED,PIN_SD_SS)) {
    DEBUG_PRINTLN("failed!");
    error_blink_loop(1);
  }
  DEBUG_PRINTLN("done.");

  uint8_t  config_part_idx;
  uint32_t data_start, data_size;
  uint32_t log_start, log_size;

  get_data_and_log_partitions(
    card,
    &config_part_idx,
    &data_start, &data_size,
    &log_start, &log_size);

#ifdef LOGGING
  /******************************
   * Prepare logging service 
   ******************************/
  if (log_start == 0) {
    DEBUG_PRINTF("Cannot find log partition id 0x%02x\r\n",
      LOG_PARTITION_ID);
    error_blink_loop(2);
  }
  DEBUG_PRINTF("Setting up log partition at block #%d, total %d bytes...",
    log_start, ui64toa(log_size*512ULL));
  if (!logger.setup(log_start,log_size,false)) {
    DEBUG_PRINTLN("failed!");
    error_blink_loop(3);
  }
  DEBUG_PRINTLN("done.");
  LOG_TS("==== Logger started ====\r\n");
#endif

  /******************************
   * Process configuration on SD
   ******************************/
  if (config_part_idx == 0) {
    LOG_TS("Cannot find configuration partition id 0x%02x\r\n",
      CONFIG_PARTITION_ID);
    error_blink_loop(4);
  }
  LOG_TS("Found configuration partition index %d\r\n", config_part_idx);
  SdVolume volume;
  if (!volume.init(card,config_part_idx)) {
    LOG_TS("Cannot open configuration volume\r\n");
    error_blink_loop(5);
  }
  LOG_TS("Reading configuration file...");
  if (!config.read_from_volume(volume)) {
    LOG("failed!\r\n");
    error_blink_loop(6);
  }
  LOG("successful.\r\n");

  /******************************
   * Prepare data storage service 
   ******************************/
  if (data_start == 0) {
    LOG_TS("Cannot find data partition id 0x%02x\r\n",
      STORAGE_PARTITION_ID);
    error_blink_loop(7);
  }
  LOG_TS("Setting up data partition at block #%d, total %d blocks..",
    data_start, data_size);
  if (!storage.setup(data_start,data_size,false)) {
    LOG("failed.\r\n");
    error_blink_loop(8);
  }
  LOG("successful.\r\n");

  //************************************************        clear storage
//  while (!storage.empty()) {
//    storage.pop();
//    Serial.println("storage not empty");
//  }
//  Serial.println("Storage EMPTY!!!!!!!!!!");
}


/***********************************************
 * 
 */
void init_radio() {
  LOG_TS("Initializing radio...");
  if (!radio.init(
      config.radio_device_address,
      config.radio_freq,
      config.radio_tx_power,
      config.long_range))
  {
    LOG("failed.\r\n");
    error_blink_loop(9);
  }
  LOG("successful; NodeId = 0x%02X\r\n", config.radio_device_address);
}

/***********************************************
 * 
 */
void error_blink_loop(uint8_t code) {
  LOG_TS("ERROR: code %d\n", code);
  for (;;) {
    LED_ON();
    delay(200);
    LED_OFF();
    delay(200);
  }
}

#if defined(ADAFRUIT_FEATHER_M0)
/***********************************************
 * temperature reading
 * see: http://ww1.microchip.com/downloads/en/AppNotes/Atmel-42645-ADC-Configurations-with-Examples_ApplicationNote_AT11481.pdf
 */
static __inline__ void syncADC() __attribute__((always_inline, unused));
static void syncADC() {
  while (ADC->STATUS.bit.SYNCBUSY == 1)
    ;
}

uint16_t read_temperature() {
  uint32_t valueRead = 0;

  SYSCTRL->VREF.bit.TSEN = 1;  // Enable temperature sensor
  ADC->INPUTCTRL.bit.MUXPOS = 0x18;  // Temperature reference
  ADC->CTRLA.bit.ENABLE = 0x01; // Enable ADC

  // Start conversion
  syncADC();
  ADC->SWTRIG.bit.START = 1;

  // Clear the Data Ready flag
  ADC->INTFLAG.reg = ADC_INTFLAG_RESRDY;

  // Start conversion again, since The first conversion after the reference is changed must not be used.
  syncADC();
  ADC->SWTRIG.bit.START = 1;

  // Store the value
  while (ADC->INTFLAG.bit.RESRDY == 0);   // Waiting for conversion to complete
  valueRead = ADC->RESULT.reg;

  syncADC();
  ADC->CTRLA.bit.ENABLE = 0x00;             // Disable ADC
  syncADC();

  return valueRead;
}
#endif

#if defined(ARDUINO_AVR_FEATHER32U4)
uint16_t read_temperature() {
  unsigned int wADC;

  // The internal temperature has to be used
  // with the internal reference of 1.1V.
  // Channel 8 can not be selected with
  // the analogRead function yet.

  // Set the internal reference and mux.
  ADMUX = (_BV(REFS1) | _BV(REFS0) | _BV(MUX3));
  ADCSRA |= _BV(ADEN);  // enable the ADC

  delay(20);            // wait for voltages to become stable.

  ADCSRA |= _BV(ADSC);  // Start the ADC

  // Detect end-of-conversion
  while (bit_is_set(ADCSRA,ADSC));

  // Reading register "ADCW" takes care of how to read ADCL and ADCH.
  wADC = ADCW;

  return wADC;
}
#endif

/*********************************************************************
 * Force watchdog reset
 */
void force_reset() {
  LOG_TS("Something went wrong; force reset\r\n");
  Watchdog.enable(1000);
  while (true)
    ;
}

/*********************************************************************
 * Custom watchdog reset to prevent very slow synchronization of SAMD chip
 */
void watchdog_reset() {
  static uint32_t last = 0;
  if ( (millis() - last) > WATCHDOG_TIMEOUT/2 ) {
    //DEBUG_PRINTLN("Reset watchdog");
    Watchdog.reset();
    last = millis();
  }
}

/*********************************************************************
 * Put the device into sleep mode for the specified number of seconds
 */
void sleeping_wait(uint16_t seconds) {
  if (seconds == 0) return;
  LOG_TS("Going to sleep for %d second(s)\r\n", seconds);
#if defined(DEBUG)
  // Cannot sleep for real in debug mode
  uint32_t ts = millis();
  wake_now = false;
  while (millis() - ts < seconds*1000) {
    watchdog_reset();
    if (wake_now) break;
  }
  wake_now = false;
#else
  uint32_t until = get_total_seconds() + seconds;
  uint32_t remain;
  while ((remain = until - get_total_seconds()) > 0) {
    if (remain > 8)
      adjustTime(Watchdog.sleep(8000)/1000);
    else if (remain > 4)
      adjustTime(Watchdog.sleep(4000)/1000);
    else if (remain > 2)
      adjustTime(Watchdog.sleep(2000)/1000);
    else if (remain > 1)
      adjustTime(Watchdog.sleep(1000)/1000);
  }
#endif
  LOG_TS("Waking up\r\n");
}

/***********************************************
 * 
 */
bool is_day() {
  if (get_year() == 2000)
    // time is not yet known, assume daytime
    return true;

  uint8_t hour_val = (get_hours() + config.time_zone) % 24;
  if ( (hour_val >= config.day_start_hour) && 
       (hour_val < config.day_end_hour))
    return true;

  return false;
}

/***********************************************
 * 
 */
class TaskReport:public virtual Task {
private:
  uint32_t _ts;
  PacketReport _pkt;
  uint8_t _send_seq;
  uint8_t _buf[64];
  uint8_t _len;
  Address _from;
  uint32_t _last_heard_gw;
  bool _ack_received;

public:
  virtual const char * get_name() {
    return "Report";
  }

  virtual TASK(run()) {

    TASK_BEGIN();

    _ts = 0;
    _send_seq = 0;
    _last_heard_gw = 0;
    for (;;) { // main task loop
      // make sure clock is synchronized
      if (get_year() == 2000) {
        LOG_TS("REPORT: Wait for clock synchronization\r\n");
        TASK_WAIT_UNTIL(get_year() != 2000);
      }
      // wait for my own time slot
//      uint32_t time_to_wait = time_wait_for_tx();
      uint32_t time_to_wait = 20;
      if (time_to_wait > 0) {
        LOG_TS("REPORT: Wait for time slot: %d seconds\r\n", time_to_wait);
        LOG_TS("REPORT: Turn off radio\r\n");
        RADIO_OFF();
        set_wakeup_time(get_total_seconds() + time_to_wait);
        TASK_WAIT_UNTIL(get_total_seconds() >= get_wakeup_time());
        reset_wakeup_time();
      }

      for (;;) { // retrieve stored records
        if (storage.empty()) {
          LOG_TS("REPORT: Wait for records\r\n");
          LOG_TS("REPORT: Turn off radio\r\n");
          RADIO_OFF();
          TASK_WAIT_UNTIL(!storage.empty());
        }
//        if (time_wait_for_tx() > 0) break; // tx slot already ended

        LOG_TS("REPORT: Record count = %d\r\n", storage.count());

        storage.top(&_pkt.report);
        _send_seq++;
        _pkt.seq = _send_seq;

        // if gateway hasn't been heard for too long, wait for ADV again
        // TODO convert 5000 to a configurable param
        if (millis() - _last_heard_gw > 5000) {
          LOG_TS("REPORT: Turn on radio\r\n");
          RADIO_ON();
          for (;;) { // wait for adv
            LOG_TS("REPORT: Awaiting ADV\r\n");
            TASK_WAIT_UNTIL(radio.available());
//            if (time_wait_for_tx() > 0) break; // tx slot already ended
            _len = sizeof(_buf);
            radio.recv(&_from, _buf, &_len);
            if ((_len == sizeof(PacketAdvertise) && 
                (_buf[0] == PKT_TYPE_ADVERTISE) &&
                ((config.radio_gateway_address == 0xff) || 
                 (_from == config.radio_gateway_address))))
            {
              PacketAdvertise* adv = (PacketAdvertise*)_buf;
              LOG_TS("REPORT: RX ADV src=0x%02x seq=%d\r\n",_from,adv->seq);
              _last_heard_gw = millis();
              break;
            }
          } // wait for adv
        }

//        if (time_wait_for_tx() > 0) break; // tx slot already ended

        _ack_received = false;
        while (!_ack_received) { // (re)transmit until acked
//          if (time_wait_for_tx() > 0) break; // tx slot already ended
          //TASK_DELAY(2000,_ts); // XXX testing
          LOG_TS("REPORT: TX seq=%d\r\n",_pkt.seq);
          radio.send(config.radio_gateway_address, (uint8_t*)&_pkt,sizeof(_pkt));
          TASK_WAIT_UNTIL(radio.send_done());
          LOG_TS("REPORT: Done; awaiting ACK\r\n");
          TASK_SHORT_BLINK(_ts,10);

          // CAUTION:
          // do not put initialization and declaration on the same line
          static uint32_t timeout;
          timeout = millis() + config.ack_timeout*1000;

          while (!_ack_received) { // wait for ack
            TASK_WAIT_UNTIL(radio.available() || (millis() > timeout));
            if (!radio.available()) {
              LOG_TS("REPORT: Timed out\r\n");
              break;
            }
            _len = sizeof(_buf);
            radio.recv(&_from, _buf, &_len);
            if ((_len == sizeof(PacketAck) && 
                (_buf[0] == PKT_TYPE_ACK) &&
                ((config.radio_gateway_address == 0xff) || 
                 (_from == config.radio_gateway_address))))
            {
              PacketAck* ack = (PacketAck*)_buf;
              LOG_TS("REPORT: RX ACK src=0x%02x seq=%d\r\n",_from,ack->seq);
              LOG_TS("REPORT: RX ACK seq=%d\r\n",_send_seq);
              _last_heard_gw = millis();
              if (_send_seq == ack->seq) {
                LOG_TS("REPORT: ACK received\r\n");
                // it is now safe to remove the acked item
                storage.pop();                                                                 //pop data from storage
                LOG_TS("REPORT: Remove record\r\n");
                _ack_received = true;
                break;
              }
            }
          } // wait for ack
        } // (re)transmit until acked
      } // retrieve stored records
    } // main task loop

    TASK_END();
  }
} taskReport;

/***********************************************
 * 
 */
class TaskLogger:public virtual Task {
private:
  uint32_t _ts;
  uint32_t _last_collected;
  uint32_t _gps_on_time;
  uint32_t _next_collection;
  uint8_t _send_seq;
  PacketReport _pkt;
  bool    _first_record;

public:
  virtual const char * get_name() {
    return "Logger";
  }

  virtual TASK(run()) {

    TASK_BEGIN();

    _ts = 0;
    _next_collection = 0;
    _last_collected = 0;
    _send_seq = 0;
    _first_record = true;
    for (;;) {
      GPS.reset_buffer(true);
      GPS_ON();
      GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
      GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
      _gps_on_time = millis();

      LOG_TS("LOGGER: Wait for fix\r\n");

      /* yield while waiting for PPS to wake MCU up */
      pps_detected = false;
      set_wakeup_time(get_total_seconds() + config.gps_max_wait_for_fix);
#if defined(ARDUINO_AVR_FEATHER32U4)
      TASK_WAIT_UNTIL(pps_detected ||
                      (get_total_seconds() >= get_wakeup_time()));
#elif defined(ADAFRUIT_FEATHER_M0)
      attachInterrupt(digitalPinToInterrupt(PIN_1PPS), pps_handler, HIGH);
      TASK_WAIT_UNTIL(pps_detected ||
                      (get_total_seconds() >= get_wakeup_time()));
      detachInterrupt(digitalPinToInterrupt(PIN_1PPS));
#endif
      reset_wakeup_time();

      if (!pps_detected) {
        LOG_TS("LOGGER: PPS not detected\r\n");
        LED_OFF(); GPS_OFF();
        // schedule time to try again
        _next_collection = get_total_seconds() + config.next_collect_no_fix;
        set_wakeup_time(_next_collection);
        TASK_WAIT_UNTIL(get_total_seconds() >= get_wakeup_time());
        continue;
      }

      // PPS signal detected; start reading and parsing NMEA
      LOG_TS("LOGGER: PPS detected\r\n");

      // reset GPS reading and flush serial buffer
      GPS.reset_buffer(true);

      // record timestamp for timeout checking
      _ts = millis();
      // keep reading GPS until all required information has been received 
      // or timed out
      while (!GPS.read()) {
        
        if ((millis() - _ts) > ((uint32_t)config.gps_max_wait_for_fix*1000) ) {
          break; // timed out
        }
        TASK_YIELD();
      }

      LED_OFF(); GPS_OFF();

      if (!GPS.read()) {
        LOG_TS("LOGGER: Cannot acquire a fix; turn off GPS\r\n");
        // schedule time to try again
        _next_collection = get_total_seconds() + config.next_collect_no_fix;
        set_wakeup_time(_next_collection);
        TASK_WAIT_UNTIL(get_total_seconds() >= get_wakeup_time());
        reset_wakeup_time();
        continue;
      }

      set_time(GPS.year+2000, GPS.month, GPS.day, GPS.hour, GPS.minute, GPS.seconds);
      LOG_TS("LOGGER: Time/loc acquired; turn off GPS\r\n");

      // if time is messed up, force reset immediately;
      // otherwise CPU may not wake up after sleep
      if (get_day() == 0 || get_month() == 0) {
        force_reset();
      }

      // to make sure every device collects location data at the same points in
      // time, the next collection time is the number of seconds since midnight
      // (UTC) that is multiple of the configured collect interval
      uint32_t second_now = GPS.hour*3600 + GPS.minute*60 + GPS.seconds;
      uint32_t time_to_fix = (millis() - _gps_on_time);
      uint16_t col_interval = is_day() ?
        config.collect_interval_day : config.collect_interval_night;
      _next_collection = get_total_seconds() + col_interval - (second_now%col_interval);

      // TODO the field "last_heard_from_gw" is now used for time to GPS fix
      // Its name should be changed.
      _pkt.report.year = GPS.year;
      _pkt.report.month = GPS.month;
      _pkt.report.day = GPS.day;
      _pkt.report.hour = GPS.hour;
      _pkt.report.minute = GPS.minute;
      _pkt.report.second = GPS.seconds;

      // if this is the first record, insert a special "boot" record first
      if (_first_record) {
        _first_record = false;
        _pkt.report.latitude = (1<<31)-1;
        _pkt.report.longitude = (1<<31)-1;
        _pkt.report.vbat = 65535;
        _pkt.report.quality = 0;
        _pkt.report.satellites = 0;
        _pkt.report.temperature = 0;
        _pkt.report.last_heard_from_gw = millis();  // will be used to derive boot time
        storage.push(_pkt.report);
        LOG_TS("LOGGER: Record pushed to storage; record count = %lu\r\n", (uint32_t)storage.count());
      }
      _pkt.report.latitude = GPS.latitude;
      _pkt.report.longitude = GPS.longitude;
      _pkt.report.vbat = ((uint32_t)analogRead(PIN_VBAT))*2*3300/1024;
      _pkt.report.quality = GPS.fixquality;
      _pkt.report.satellites = GPS.satellites;
      _pkt.report.temperature = read_temperature();
      _pkt.report.last_heard_from_gw = time_to_fix;

      //                                                                                                                        get mpu data
      get_accelerometer();
      _pkt.report.ax = glo_ax;
      _pkt.report.ay = glo_ay;
      _pkt.report.az = glo_az;
      _pkt.report.glo_ag_x = glo_ag_x;
      _pkt.report.glo_ag_y = glo_ag_y;
      _pkt.report.glo_ag_z = glo_ag_z;

      storage.push(_pkt.report);
      LOG_TS("LOGGER: Record pushed to storage; record count = %lu\r\n", (uint32_t)storage.count());

      // -----------------
      // TODO: investigate
      // -----------------
      // Why is this line needed?  This line can reduce the power consumption to
      // 0.9 mA.  Without it the power consumption is around 2-3 mA.
      //
      // SD library always pull CS high after every read/write operation anyway.
      digitalWrite(PIN_SD_SS,HIGH);

#if !defined(ADALOGGER)
      // in non-ack mode, send logged data to gateway right away in the next
      // tx slot
      if (!config.use_ack) {
        _send_seq++;
        _pkt.seq = _send_seq;

        static uint32_t wait_time;
        wait_time = time_wait_for_tx();
        if (wait_time < 0) {
          // transmission in non-ack mode may take a long time, especially with
          // long range enabled and multiple tx repeats, so the node should make
          // sure it has a whole time slot available
          wait_time += config.slot_interval;
        }
        set_wakeup_time(get_total_seconds() + time_wait_for_tx());
        TASK_WAIT_UNTIL(get_total_seconds() >= get_wakeup_time());
        reset_wakeup_time();

        for (uint8_t i=0; i<config.tx_repeat; i++) {
          watchdog_reset();
          LOG_TS("LOGGER: Send packet with seq=%d to gateway\r\n", _pkt.seq);
          radio.send(config.radio_gateway_address,(uint8_t*)&_pkt,sizeof(_pkt));
          TASK_WAIT_UNTIL(radio.send_done());
          SHORT_BLINK(50,100);
        }
        RADIO_OFF();
      }
#endif

      // wake up earlier to compensate more time to wait for fix, using the
      // current round's wait time as a prediction
      _next_collection -= time_to_fix/1000;
      set_wakeup_time(_next_collection);
      LOG_TS("LOGGER: Time to wait until next round: %ld\r\n", _next_collection - get_total_seconds());
      TASK_WAIT_UNTIL(get_total_seconds() >= get_wakeup_time());
      reset_wakeup_time();
    }

    TASK_END();
  }
} taskLogger;

/***********************************************
 * 
 */
#if defined(ARDUINO_AVR_FEATHER32U4)
ISR (PCINT0_vect) { // handle pin change interrupt for D8 to D13
#elif defined(ADAFRUIT_FEATHER_M0)
void pps_handler() {
#endif
  if (digitalRead(PIN_1PPS) == HIGH) {
    pps_detected = true;
#ifdef DEBUG
    wake_now = true;
#endif
  }
}

/***********************************************
 * 
 */
void setup() {
#if defined(ADAFRUIT_FEATHER_M0)
  uint8_t device_model = 0x01;
  uint8_t reset_flags = REG_PM_RCAUSE;
#elif defined(ARDUINO_AVR_FEATHER32U4)
  uint8_t device_model = 0x02;
  uint8_t reset_flags = MCUSR;
#endif
  pinMode(LED_BUILTIN,OUTPUT);
  pinMode(PIN_1PPS,INPUT_PULLUP);
  pinMode(PIN_GPS_EN,OUTPUT);
  pinMode(PIN_RFM95_CS,OUTPUT);
  digitalWrite(PIN_RFM95_CS,HIGH);
  GPS_SERIAL.begin(9600);

  LED_OFF(); GPS_OFF();
#ifdef DEBUG
  Serial.begin(9600);
  while (!Serial)
    ; // wait for serial port to connect. Needed for native USB port only
#endif
  LED_ON();
  init_time();
  init_card();
  init_radio();

  LOG_TS("Device initialized with parameters:\r\n");
  LOG(" * Firmware version: %d\r\n", FIRMWARE_VERSION);
  LOG(" * Device model: 0x%02x\r\n", device_model);
  LOG(" * Reset flags: 0x%02x\r\n", reset_flags);
  LOG(" * radio_freq = %d.%02d MHz\r\n",
    (int)config.radio_freq,
    (int)(config.radio_freq*100) % 100);
  LOG(" * radio_tx_power = %d dBm\r\n", config.radio_tx_power);
  LOG(" * radio_device_address = %d (0x%02X)\r\n",
    config.radio_device_address,
    config.radio_device_address);
  LOG(" * radio_gateway_address = %d (0x%02X)\r\n",
    config.radio_gateway_address,
    config.radio_gateway_address);
  LOG(" * collect_interval_day = %d sec\r\n", config.collect_interval_day);
  LOG(" * collect_interval_night = %d sec\r\n", config.collect_interval_night);
  LOG(" * day_start_hour = %d\r\n", config.day_start_hour);
  LOG(" * day_end_hour = %d\r\n", config.day_end_hour);
  LOG(" * time_zone = %d hours\r\n", config.time_zone);
  LOG(" * use_ack = %d\r\n", config.use_ack);
  LOG(" * ack_timeout = %d sec\r\n", config.ack_timeout);
  LOG(" * long_range = %d\r\n", config.long_range);
  LOG(" * tx_repeat = %d\r\n", config.tx_repeat);
  LOG(" * gps_max_wait_for_fix = %d sec\r\n", config.gps_max_wait_for_fix);
  LOG(" * next_collect_no_fix = %d sec\r\n", config.next_collect_no_fix);
  LOG(" * total_slots = %d\r\n", config.total_slots);
  LOG(" * slot_interval = %d sec\r\n", config.slot_interval);
  LOG(" * report size = %d bytes\r\n", sizeof(ReportItem));

#ifndef DEBUG
  USBDevice.detach();
#endif

  Watchdog.enable(WATCHDOG_TIMEOUT);

  // send boot report right away
  PacketBoot pkt;
  pkt.firmware = FIRMWARE_VERSION;
  pkt.device_model = device_model;
  pkt.reset_flags = reset_flags;
  memcpy(&pkt.config, &config, sizeof(config));
  radio.send(config.radio_gateway_address,(uint8_t*)&pkt,sizeof(pkt));
  Serial.println("Send Boot");

  while (!radio.send_done()) {
    watchdog_reset();
  }

//  while(!radio.available()){
//    ;
//  }
//  uint8_t _buf[64];
//  uint8_t _len;
//  Address _from;
//  _len = sizeof(_buf);
//  radio.recv(&_from, _buf, &_len);
//  if ((_len == sizeof(PacketAck) && 
//       (_buf[0] == PKT_TYPE_ACK) &&
//         ((config.radio_gateway_address == 0xff) || 
//             (_from == config.radio_gateway_address))))
//            {
//              PacketAck* ack = (PacketAck*)_buf;
//              LOG_TS("REPORT: RX ACK src=0x%02x seq=%d\r\n",_from,ack->seq);
//            }
//  radio.recv(&_from, _buf, &_len);
//  if ((_len == sizeof(PacketAdvertise) && 
//                (_buf[0] == PKT_TYPE_ADVERTISE) &&
//                ((config.radio_gateway_address == 0xff) || 
//                 (_from == config.radio_gateway_address))))
//   {
//      PacketAdvertise* adv = (PacketAdvertise*)_buf;
//      LOG_TS("REPORT: RX ADV src=0x%02x seq=%d\r\n",_from,adv->seq);
//   } else{
//    Serial.println("NOT ADV");
//   }
  



  if (config.use_ack) {
    GPS_ON();
  }
  else {
    RADIO_OFF();
  }

  LED_OFF();

  if (config.use_ack) {
    num_tasks = 2;
    tasks[0] = &taskLogger;
    tasks[1] = &taskReport;
  }
  else {
    num_tasks = 1;
    tasks[0] = &taskLogger;
  }
}

/***********************************************
 * 
 */
void loop() {
  uint32_t min_wakeup_time = (uint32_t)-1;
  uint32_t wakeup_time;
  uint32_t current;
  for (uint8_t i=0; i<num_tasks; i++) {
    // always run a task no matter it is sleeping or not; in case it needs
    // to be woken up by some other source
    tasks[i]->run();
    // determine the earliest wakeup time among all the tasks
    wakeup_time = tasks[i]->get_wakeup_time();
    if (wakeup_time < min_wakeup_time) {
      min_wakeup_time = wakeup_time;
    }
  }
  // if there is some time before the earliest wakeup time, put the CPU to sleep
  current = get_total_seconds();
  if (min_wakeup_time >= current) {
    sleeping_wait(min_wakeup_time - current);
  }
  watchdog_reset();
}

/***********************************************
 * Return the number of seconds to wait for the allowed transmission slot.
 * A negative value indicates the number of seconds elapsed since the
 * beginning of the current slot.
 *
 * Currently it is based on a deterministic time point calculated from the
 * node address.
 */
int16_t time_wait_for_tx() {
  uint32_t current_time = get_total_seconds();
  uint32_t current_slot = current_time/config.slot_interval;
  uint16_t my_slot = config.radio_device_address % config.total_slots;
  int16_t slots_to_wait = 
    (my_slot-current_slot+config.total_slots) % config.total_slots;
  int16_t time_to_wait =
    (slots_to_wait+current_slot)*config.slot_interval - current_time;
  return time_to_wait;
}
