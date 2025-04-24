#pragma once

class Pollable
{
   public:
      virtual ~Pollable() {}

      /**
       * @return the filedescriptor that can be used with poll() and select()
       */
      virtual int getFD() const = 0;
};

