#ifndef POLLABLE_H_
#define POLLABLE_H_

class Pollable
{
   public:
      virtual ~Pollable() {}

      /**
       * @return the filedescriptor that can be used with poll() and select()
       */
      virtual int getFD() const = 0;
};

#endif /*POLLABLE_H_*/
