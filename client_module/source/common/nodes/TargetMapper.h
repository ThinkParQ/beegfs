#ifndef TARGETMAPPER_H_
#define TARGETMAPPER_H_

#include <app/App.h>
#include <common/toolkit/list/UInt16ListIter.h>
#include <common/Common.h>
#include <common/threading/RWLock.h>
#include <common/toolkit/Serialization.h>
#include <common/toolkit/StringTk.h>
#include <common/Types.h>

#include <linux/rbtree.h>


struct TargetMapper;
typedef struct TargetMapper TargetMapper;


extern void TargetMapper_init(TargetMapper* this);
extern TargetMapper* TargetMapper_construct(void);
extern void TargetMapper_uninit(TargetMapper* this);
extern void TargetMapper_destruct(TargetMapper* this);

extern bool TargetMapper_mapTarget(TargetMapper* this, uint16_t targetID,
   NumNodeID nodeID);

extern void TargetMapper_syncTargets(TargetMapper* this, struct list_head* mappings);
extern void TargetMapper_getTargetIDs(TargetMapper* this, UInt16List* outTargetIDs);

extern NumNodeID TargetMapper_getNodeID(TargetMapper* this, uint16_t targetID);

struct TargetMapper
{
   RWLock rwlock;

/* private: */
   struct rb_root _entries; /* TargetMapping */
};

#endif /* TARGETMAPPER_H_ */
