#include <stdint.h>
#include <vector>
class Tenkey
{
public:
  virtual std::vector<uint8_t> GetPushState()const = 0;
  virtual bool IsPushError()const = 0;

  virtual void Update() = 0;

  static Tenkey* Create();
  virtual ~Tenkey(){}
};