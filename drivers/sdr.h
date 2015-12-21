#include <stdint.h>
#include <vector>
#include <array>
#include <deque>
class Sdr
{
public:
  virtual void Peek(uint8_t *sample, int length) = 0;
  virtual void Spectrum(float *sample, int length,int fold=256) = 0;
  virtual uint32_t Tune(float Mhz) = 0;
  static Sdr* Create();
  virtual ~Sdr(){}

};