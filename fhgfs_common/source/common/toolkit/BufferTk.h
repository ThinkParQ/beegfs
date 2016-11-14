#ifndef BUFFERTK_H_
#define BUFFERTK_H_


#include <common/Common.h>


class BufferTk
{
   public:
      static uint32_t hash32(const char* data, int len);


   private:
      BufferTk() {}
};


#endif /* BUFFERTK_H_ */
