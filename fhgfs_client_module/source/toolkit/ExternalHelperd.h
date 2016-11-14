#ifndef EXTERNALHELPERD_H_
#define EXTERNALHELPERD_H_

#include <app/config/Config.h>
#include <app/App.h>
#include <common/toolkit/StringTk.h>
#include <common/threading/Mutex.h>
#include <common/Common.h>


// Note: We have to be very careful with every operation of the helperd: They all must be direct
//    and may not result (e.g. indirectly) in another call to one of the ExternalHelperd_... calls,
//    because that would lead to a deadlock if no further connections to the helperdNode are
//    available. (The only exception is logging, because the logger creates its own node instance.)
// Note2: If the above requirement cannot be fulfilled, because the used frameworks (e.g. the
//    MessagingTk) cannot live without the internal indirect call to the ExternalHelperd_...(),
//    the only option is to copy the code of the corresponding framework and strip all the
//    problem-code from it.


// forward declarations...

struct ExternalHelperd;
typedef struct ExternalHelperd ExternalHelperd;

struct Node;


extern void ExternalHelperd_init(ExternalHelperd* this, App* app, Config* cfg);
extern ExternalHelperd* ExternalHelperd_construct(App* app, Config* cfg);
extern void ExternalHelperd_uninit(ExternalHelperd* this);
extern void ExternalHelperd_destruct(ExternalHelperd* this);

extern void ExternalHelperd_initHelperdNode(App* app, Config* cfg, const char* nodeID,
   struct Node** outNode);
extern unsigned ExternalHelperd_dropConns(ExternalHelperd* this);

extern void __ExternalHelperd_logUnreachableHelperd(ExternalHelperd* this);
extern void __ExternalHelperd_logReachableHelperd(ExternalHelperd* this);

extern char* ExternalHelperd_getHostByName(ExternalHelperd* this, const char* hostname);


struct ExternalHelperd
{
   App* app;

   struct Node* helperdNode;
   bool helperdReachable;

   Mutex mutex; // protects the helperdReachable member variable (see notes below)
};

// Additional Notes:
// - mutex: There is a small race condition concerning the helperdReachable variable, that could
//    lead to the (almost irrelevant!) problem, that an unreachable helperd is logged when it
//    is already available again. However, with the next call, the availability will also be logged
//    and all calls will work correctly in any situation (independent of the wrong log messages).
//    The problem comes from the fact that it is not possible to lock the mutex around the
//    MessagingTk calls, because in that case, a _getHostByName() call would deadlock when the
//    MessagingTk tries to _log() a message.
//    The current implementation is absolutely acceptable, even for production use!!!

#endif /*EXTERNALHELPERD_H_*/
