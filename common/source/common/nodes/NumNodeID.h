#ifndef NUMNODEID_H
#define NUMNODEID_H

#include <common/NumericID.h>

/*
 * represents a numeric node ID
 *
 * Note: this must always be in sync with client's NumNodeId!
 *
 */
enum NumNodeIDTag {};
typedef NumericID<uint32_t, NumNodeIDTag> NumNodeID;

typedef std::list<NumNodeID> NumNodeIDList;
typedef NumNodeIDList::iterator NumNodeIDListIter;
typedef NumNodeIDList::const_iterator NumNodeIDListCIter;

typedef std::vector<NumNodeID> NumNodeIDVector;
typedef NumNodeIDVector::iterator NumNodeIDVectorIter;
typedef NumNodeIDVector::const_iterator NumNodeIDVectorCIter;

#endif /*NUMNODEID_H*/
