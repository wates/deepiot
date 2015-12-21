#include <stdint.h>

class Os
{
public:
  virtual void Poll() = 0;
  virtual void Flash(uint32_t color, int duration = 3) = 0;

  static Os* Create();
  virtual ~Os(){}
};

