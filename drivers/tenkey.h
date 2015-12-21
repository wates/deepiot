#include <stdint.h>
#include <vector>
#include <deque>
class Tenkey
{
public:
  virtual std::vector<uint8_t> GetPushState()const = 0;
  virtual bool IsPushError()const = 0;
  virtual const std::deque<char>& Ascii()const = 0;

  virtual void Update() = 0;

  static Tenkey* Create();
  virtual ~Tenkey(){}
};