#ifndef DELAYEDDISPOSER_H
#define DELAYEDDISPOSER_H

#include <string>

class DelayedDisposer
{
public:
    DelayedDisposer();
    static int unlink(std::string fname, unsigned delay);
};

#endif // DELAYEDDISPOSER_H
