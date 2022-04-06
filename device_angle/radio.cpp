#include <SPI.h>
#include <RH_RF95.h>

#include "radio.h"

/***********************************************
 * 
 */
Radio::Radio():_rf95(PIN_RFM95_CS,PIN_RFM95_INT) {
  // make sure the RFM module is DISABLED before initializing
  // hopefully the RadioHead library will take care of the CS pin
  pinMode(PIN_RFM95_CS,OUTPUT);
  digitalWrite(PIN_RFM95_CS,HIGH);
}

/***********************************************
 * 
 */
bool Radio::init(Address my_addr,float freq,int8_t tx_power,uint8_t long_range) {
  this->reset();
  if (!_rf95.init())
    return false;
  if (!_rf95.setFrequency(freq))
    return false;
  _rf95.setTxPower(tx_power, false);
  _rf95.setThisAddress(my_addr);
  _rf95.setPromiscuous(false);
  if (long_range) {
    RH_RF95::ModemConfig config = { 
      // See Table 86 for LoRa for more info
      //0x88, // Reg 0x1D: BW=250kHz, Coding=4/8, Header=explicit
      //0xc4, // Reg 0x1E: Spread=4096chips/symbol, CRC=enable
      //0x00  // Reg 0x26: Mobile=Off, Agc=Off
      0x78, // Reg 0x1D: BW=125kHz, Coding=4/8, Header=explicit
      0xc4, // Reg 0x1E: Spread=4096chips/symbol, CRC=enable
      0x0c  // Reg 0x26: Mobile=On, Agc=On
    };
    _rf95.setModemRegisters(&config);
    //_rf95.setModemConfig(RH_RF95::Bw125Cr48Sf4096); // slow-speed long-range
  }
  _my_addr = my_addr;
  return true;
}

/***********************************************
 * 
 */
void Radio::reset() {
  pinMode(PIN_RFM95_RST,OUTPUT);
  digitalWrite(PIN_RFM95_RST,LOW);
  delay(10);
  pinMode(PIN_RFM95_RST,INPUT); // based on datasheet
  digitalWrite(PIN_RFM95_RST,HIGH);
  delay(10);
}

/***********************************************
 * 
 */
bool Radio::available() {
  return _rf95.available();
}

/***********************************************
 * 
 */
void Radio::send(Address dst, const uint8_t* pkt, uint8_t pkt_len) {
  _rf95.setHeaderTo(dst);
  _rf95.setHeaderFrom(_my_addr);
  _rf95.send(pkt,pkt_len);
}

/***********************************************
 * 
 */
bool Radio::send_done() {
  return _rf95.mode() != _rf95.RHModeTx;
}

/***********************************************
 * 
 */
void Radio::recv(Address *src, uint8_t* buf, uint8_t* len) {
  *src = _rf95.headerFrom();
  _rf95.recv(buf,len);
  last_rssi = _rf95.lastRssi();
}
