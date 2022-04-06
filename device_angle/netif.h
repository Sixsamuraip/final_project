#ifndef __NETIF_H__
#define __NETIF_H__

class NetworkInterface {
public:
  typedef uint8_t Address;
  virtual bool available() = 0;
  virtual void send(Address dst, const uint8_t* pkt, uint8_t pkt_len) = 0;
  virtual bool send_done() = 0;
  virtual void recv(Address *src, uint8_t* buf, uint8_t* len) = 0;
};

typedef NetworkInterface::Address Address;

#endif

