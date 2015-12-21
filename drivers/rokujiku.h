#include <stdint.h>
#include <vector>
#include <deque>
class Rokujiku
{
public:
  virtual std::vector<uint8_t> Hex()const = 0;
  virtual void Activate() = 0;

  virtual void Update() = 0;

  static Rokujiku* Create();
  virtual ~Rokujiku(){}
};