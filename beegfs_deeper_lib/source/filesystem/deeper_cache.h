#ifndef FILESYSTEM_DEEPER_CACHE_H_
#define FILESYSTEM_DEEPER_CACHE_H_


#include <deeper/deeper_cache.h>

#include <stdlib.h>



#ifdef __cplusplus
extern "C" {
#endif

void deeper_cache_init();
void deeper_cache_destruct();

#ifdef __cplusplus
} // extern "C"
#endif


__attribute__((constructor)) void create_deeper_cache()
{
   deeper_cache_init();
}

__attribute__((destructor)) void destruct_deeper_cache()
{
   deeper_cache_destruct();
}

#endif /* FILESYSTEM_DEEPER_CACHE_H_ */
