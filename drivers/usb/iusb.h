#ifndef _IUSB_H_
#define _IUSB_H_

#include <stdint.h>
#include <vector>
#include <guiddef.h>
#include <functional>

class NakedUsb{
public:
  enum class EndpointType{
    Bulk,
    Interrupt
  };
  struct Endpoint{
    int address;
    bool input;
    int max_size;
    int interval;
    EndpointType type;
  };

  virtual bool Open(LPGUID interface_guid) = 0;
  virtual void Close() = 0;
  virtual const std::vector<Endpoint>& Endpoints()const = 0;

  virtual bool onEasyRead(std::function<void(uint8_t *buf, size_t len)> cb) = 0;
  virtual void Poll() = 0;

  static NakedUsb* Create();
};

#endif