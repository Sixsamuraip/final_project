#ifndef __RADIO_H__
#define __RADIO_H__

#include <SPI.h>
#include <RH_RF95.h>
#include "config.h"
#include "report.h"
#include "netif.h"
#include "io.h"

#define PKT_TYPE_ADVERTISE  0x01
#define PKT_TYPE_REPORT     0x02
#define PKT_TYPE_ACK        0x03
#define PKT_TYPE_BOOT       0x04

struct PacketAdvertise {
  uint8_t type;
  uint8_t seq;
  PacketAdvertise():type(PKT_TYPE_ADVERTISE) {}
} __attribute__((packed));

struct PacketReport {
  uint8_t type;
  uint8_t seq;
  ReportItem report;
  PacketReport():type(PKT_TYPE_REPORT) {}
} __attribute__((packed));

struct PacketAck {
  uint8_t type;
  uint8_t seq;
  PacketAck():type(PKT_TYPE_ACK) {}
} __attribute__((packed));

struct PacketBoot {
  uint8_t type;
  uint16_t firmware;
  uint8_t device_model;
  uint8_t reset_flags;
  Config config;
  PacketBoot():type(PKT_TYPE_BOOT) {}
} __attribute__((packed));


class Radio : NetworkInterface {
public:
  Radio();
  void reset();
  bool init(Address my_addr,float freq,int8_t tx_power,uint8_t long_range);
  bool available();
  void send(Address dst, const uint8_t* pkt, uint8_t pkt_len);
  bool send_done();
  void recv(Address *src, uint8_t* buf, uint8_t* len);
  void sleep() { _rf95.sleep(); }
  void idle()  { _rf95.setModeIdle(); }
  //void setTxPower(int8_t power) { _rf95.setTxPower(power); }
  //void setFrequency(float freq) { _rf95.setFrequency(freq); }
  //void setThisAddress(uint8_t addr) { _rf95.setThisAddress(addr); _my_addr = addr; }

  int16_t last_rssi;

private:
  RH_RF95 _rf95;
  Address _my_addr;
};

#endif
