#ifndef FLUSHER_H_
#define FLUSHER_H_

#include <app/log/Logger.h>
#include <toolkit/InodeRefStore.h>


/*
 * This component performs async cache flushes in buffered file cache mode.
 *
 * Note: Flushing needs to happen in a dedicated thread to avoid blocking other threads during
 * retries while a server is unreachable.
 */


struct Flusher;
typedef struct Flusher Flusher;


extern void Flusher_init(Flusher* this, App* app);
extern Flusher* Flusher_construct(App* app);
extern void Flusher_uninit(Flusher* this);
extern void Flusher_destruct(Flusher* this);

extern void _Flusher_requestLoop(Flusher* this);

extern void __Flusher_run(Thread* this);
void __Flusher_flushBuffers(Flusher* this);



struct Flusher
{
   Thread thread; // base class

   App* app;
};

#endif /* FLUSHER_H_ */
