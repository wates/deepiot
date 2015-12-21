#include "rokujiku.h"

#include "usb/nusb.h"

#include <thread>

// {095764F0-CDF0-4890-A521-15D8670CF1C8}
static const GUID kGuidRokujiku =
{ 0x95764f0, 0xcdf0, 0x4890, { 0xa5, 0x21, 0x15, 0xd8, 0x67, 0xc, 0xf1, 0xc8 } };

static const uint8_t kAct[] = { 0x01, 0x00, 0xff, 0x00, 0xff,
0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0xff, 0x27, 0x10, 0x00, 0x32, 0xff, 0x27, 0x10, 0x00, 0x32,
0xff, 0x27, 0x10, 0x00, 0x32, 0xff, 0x27, 0x10, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

class RokujikuImpl
  :public Rokujiku
{
public:
  virtual std::vector<uint8_t> Hex()const{
    std::vector<uint8_t> down;
    for (int i = 0; i < last_packet_.size() && 1 < last_packet_[i]; i++){
      down.push_back(last_packet_[i]);
    }

    return down;
  }
  virtual void Update(){
    usb_->Poll();
  }
  virtual void Activate(){
    const auto &ep = usb_->Endpoints();
    for (auto i : ep){
      if (!i.input){
        usb_->WriteSync(&i, kAct, sizeof(kAct));
        break;
      }
    }
  }

  bool Init(){
    GUID gp = kGuidRokujiku;
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
    });
    return true;
  }
  ~RokujikuImpl(){
    delete usb_;
  }
private:
  NakedUsb *usb_;
  std::vector<uint8_t> befor_packet_;
  std::vector<uint8_t> last_packet_;
  std::deque<char> ascii_;
};

Rokujiku* Rokujiku::Create(){
  RokujikuImpl *sa = new RokujikuImpl;
  if (sa->Init()){
    return sa;
  }
  delete sa;
  return nullptr;
}
