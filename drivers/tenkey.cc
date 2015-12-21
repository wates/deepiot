#include "tenkey.h"

#include "usb/nusb.h"

// {B86DD843-E674-4CB7-8181-5272CA862185}
static const GUID kGuidTenkey =
{ 0xb86dd843, 0xe674, 0x4cb7, { 0x81, 0x81, 0x52, 0x72, 0xca, 0x86, 0x21, 0x85 } };

static char kKeypad[] = { '/', '*', '-', '+'
, '\n', '1', '2', '3'
, '4', '5', '6', '7'
, '8', '9', '0', '.' };

class TenkeyImpl
  :public Tenkey
{
public:
  virtual std::vector<uint8_t> GetPushState()const{
    std::vector<uint8_t> down;
    for (int i = 2; i < last_packet_.size() && 1 < last_packet_[i]; i++){
      down.push_back(last_packet_[i]);
    }

    return down;
  }
  virtual bool IsPushError()const{
    return false;
  }

  virtual const std::deque<char>& Ascii()const{
    return ascii_;
  }

  virtual void Update(){
    usb_->Poll();
  }

  bool Init(){
    GUID gp = kGuidTenkey;
    usb_ = NakedUsb::Create();
    usb_->Open(&gp);
    usb_->onEasyRead([this](uint8_t *buf, size_t len){
      befor_packet_ = last_packet_;
      last_packet_.assign(buf, buf + len);
      for (auto j : last_packet_){
        auto hit = std::find(befor_packet_.begin(), befor_packet_.end(), j);
        if (befor_packet_.end() != hit){
          befor_packet_.erase(hit);
        }
      }
      for (auto i : befor_packet_){
        if (1 < i){
          if (0 <= i - 84 && i - 84 < sizeof(kKeypad))
            ascii_.push_back(kKeypad[i - 84]);
          if (42 == i) //back space
            ascii_.pop_back();
        }
        while (100 < ascii_.size())
          ascii_.pop_front();
      }
    });
    return true;
  }
  ~TenkeyImpl(){
    delete usb_;
  }
private:
  NakedUsb *usb_;
  std::vector<uint8_t> befor_packet_;
  std::vector<uint8_t> last_packet_;
  std::deque<char> ascii_;
};

Tenkey* Tenkey::Create(){
  TenkeyImpl *tk = new TenkeyImpl;
  if (tk->Init()){
    return tk;
  }
  delete tk;
  return nullptr;
}
