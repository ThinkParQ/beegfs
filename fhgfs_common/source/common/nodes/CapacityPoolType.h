#ifndef CAPACITYPOOLTYPE_H_
#define CAPACITYPOOLTYPE_H_

/**
 * note: these values are used as array index, so they must be sequential and zero-based
 */
enum CapacityPoolType
{
   CapacityPool_NORMAL=0,  // lot of free space
   CapacityPool_LOW,       // getting low on free space
   CapacityPool_EMERGENCY, // extremely low on free space or erroneous

   CapacityPool_END_DONTUSE // the final value to define array size
};

#endif // CAPACITYPOOLTYPE_H_
