#include "tenkey.h"

#include "usb/iusb.h"

// {B86DD843-E674-4CB7-8181-5272CA862185}
static const GUID kGuidTenkey =
{ 0xb86dd843, 0xe674, 0x4cb7, { 0x81, 0x81, 0x52, 0x72, 0xca, 0x86, 0x21, 0x85 } };

class TenkeyImpl
  :public Tenkey
{
public:
  virtual std::vector<uint8_t> GetPushState()const{
    std::vector<uint8_t> down;
    for (int i = 2; i < last_packet.size() && 1 < last_packet[i]; i++){
      down.push_back(last_packet[i]);
    }

    return down;
  }
  virtual bool IsPushError()const{
    return false;
  }

  virtual void Update(){
    usb->Poll();
  }

  bool Init(){
    GUID gp = kGuidTenkey;
    usb=NakedUsb::Create();
    usb->Open(&gp);
    usb->onEasyRead([this](uint8_t *buf, size_t len){
      this->last_packet.assign(buf, buf + len);
      //for (int i = 0; i < len; i++){
      //  printf("%02X,", buf[i]);
      //}
      //printf("\r\n");

    });
    return true;
  }
  ~TenkeyImpl(){
    delete usb;
  }
private:
  NakedUsb *usb;
  std::vector<uint8_t> last_packet;
};

Tenkey* Tenkey::Create(){
  TenkeyImpl *tk = new TenkeyImpl;
  if (tk->Init()){
    return tk;
  }
  delete tk;
  return nullptr;
}
