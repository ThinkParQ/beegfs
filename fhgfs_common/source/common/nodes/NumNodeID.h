#ifndef NUMNODEID_H
#define NUMNODEID_H

#include <common/NumericID.h>

/*
 * represents a numeric node ID
 *
 * this inherits from NumericID and is not a simple typedef to achieve "type" safety with nodeIDs,
 * groupIDs, targetIDs, etc. (which might in fact all be based on the same base integer type, but
 * should be distinguishable)
 *
 * Note: this must always be in sync with client's NumNodeId!
 *
 */
class NumNodeID: public NumericID<uint32_t>
{
   public:
      NumNodeID() : NumericID<uint32_t>() { }
      explicit NumNodeID(uint32_t value) : NumericID<uint32_t>(value) { }
};

typedef std::list<NumNodeID> NumNodeIDList;
typedef NumNodeIDList::iterator NumNodeIDListIter;
typedef NumNodeIDList::const_iterator NumNodeIDListCIter;

typedef std::vector<NumNodeID> NumNodeIDVector;
typedef NumNodeIDVector::iterator NumNodeIDVectorIter;
typedef NumNodeIDVector::const_iterator NumNodeIDVectorCIter;

#endif /*NUMNODEID_H*/
