#ifndef LISTTK_H_
#define LISTTK_H_

#include <common/Common.h>
#include <common/net/sock/NicAddressList.h>
#include <common/nodes/ConnectionList.h>
#include <common/toolkit/list/StrCpyList.h>
#include <common/toolkit/vector/StrCpyVec.h>
#include <common/toolkit/vector/UInt16Vec.h>


extern void ListTk_cloneNicAddressList(NicAddressList* nicList, NicAddressList* nicListClone);
extern void ListTk_cloneSortNicAddressList(NicAddressList* nicList, NicAddressList* nicListClone,
      StrCpyList* preferences);
extern void ListTk_copyUInt16ListToVec(UInt16List* srcList, UInt16Vec* destVec);

extern void ListTk_kfreeNicAddressListElems(NicAddressList* nicList);

extern bool ListTk_listContains(char* searchStr, StrCpyList* list, ssize_t* outPos);


#endif /*LISTTK_H_*/
