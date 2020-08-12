#ifndef CLIENTOPS_H_
#define CLIENTOPS_H_

#include <common/nodes/Node.h>
#include <common/threading/Mutex.h>

#include <unordered_map>


/**
* Collects client ops data mapped to an id (ip/user) and calculates diffs and sums.
*/
class ClientOps
{
   public:
      using OpsList = std::list<int64_t>;
      using IdOpsMap = std::map<int64_t, OpsList>;

      bool addOpsList(uint64_t id, const OpsList& opsList);

      IdOpsMap getDiffOpsMap() const;
      OpsList getDiffSumOpsList() const;

      const IdOpsMap& getAbsoluteOpsMap() const
      {
         return idOpsMap;
      }

      const OpsList& getAbsoluteSumOpsList() const
      {
         return sumOpsList;
      }

      void clear();


   protected:
      void sumOpsListValues(OpsList& list1, const OpsList& list2) const;

      // helper callback functions
      static uint64_t sum(uint64_t a, uint64_t b)
      {
         return a + b;
      }

      static uint64_t diff(uint64_t a, uint64_t b)
      {
         return a - b;
      }

      IdOpsMap idOpsMap;
      IdOpsMap oldIdOpsMap;
      OpsList sumOpsList;
      OpsList oldSumOpsList;
      Mutex idOpsMapMutex;
};

/**
* Helper class to request client ops from nodes.
*/
class ClientOpsRequestor
{
   public:
      typedef std::unordered_map<uint64_t, ClientOps::OpsList> IdOpsUnorderedMap;

      static IdOpsUnorderedMap request(Node& node, bool perUser);
};

#endif
