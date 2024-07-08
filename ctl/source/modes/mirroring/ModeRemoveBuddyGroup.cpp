#include <app/App.h>
#include <common/net/message/nodes/RemoveBuddyGroupMsg.h>
#include <common/net/message/nodes/RemoveBuddyGroupRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>

#include "ModeRemoveBuddyGroup.h"


#define ARG_MIRROR_GROUP_ID "--mirrorgroupid"
#define ARG_NODE_TYPE       "--nodetype"
#define ARG_DRY_RUN         "--dry-run"
#define ARG_FORCE           "--force"

int ModeRemoveBuddyGroup::execute()
{
   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   ModeHelper::printEnterpriseFeatureMsg();

   if (!ModeHelper::checkRootPrivileges())
      return APPCODE_RUNTIME_ERROR;

   // check arguments
   StringMapIter iter;

   iter = cfg->find(ARG_MIRROR_GROUP_ID);
   if (iter == cfg->end())
   {
      std::cerr << "Missing argument: " ARG_MIRROR_GROUP_ID << std::endl;
      return APPCODE_INVALID_CONFIG;
   }
   else if (!StringTk::isNumeric(iter->second))
   {
      std::cerr << "Invalid value for " ARG_MIRROR_GROUP_ID << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   const uint16_t groupID = StringTk::strToUInt(iter->second.c_str());

   cfg->erase(iter);

   const NodeType nodeType = ModeHelper::nodeTypeFromCfg(cfg);
   if (nodeType != NODETYPE_Storage)
   {
      std::cerr << "Invalid value for " ARG_NODE_TYPE " (must be `storage')" << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   iter = cfg->find(ARG_DRY_RUN);

   const bool dryRun = iter != cfg->end();
   if (dryRun)
      cfg->erase(iter);

   iter = cfg->find(ARG_FORCE);

   const bool force = iter != cfg->end();
   if (force)
      cfg->erase(iter);

   if (ModeHelper::checkInvalidArgs(cfg))
      return APPCODE_INVALID_CONFIG;

   const FhgfsOpsErr removeRes = removeGroup(groupID, dryRun, force);

   if (removeRes == FhgfsOpsErr_SUCCESS)
   {
      std::cout << "Operation successful." << std::endl;
      if (force)
      {
         std::cout << "Warning: Using --force to remove non empty buddy groups is "
               << "strongly discouraged since it leads to an inconsistent system "
               << "and inaccessible data." << std::endl;
      }
      return APPCODE_NO_ERROR;
   }
   else if (removeRes == FhgfsOpsErr_NOTEMPTY)
   {
      std::cout << "Could not remove buddy group: the group still contains data. " << std::endl
            << "Non empty buddy groups can't be removed because that would lead to an "
            << "inconsistent system and inaccessible data." << std::endl;

      return APPCODE_RUNTIME_ERROR;
   }
   else
   {
      std::cerr << "Could not remove buddy group: " << removeRes << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }
}

void ModeRemoveBuddyGroup::printHelp()
{
   std::cout <<
      "MODE ARGUMENTS:\n"
      " Mandatory:\n"
      "  --mirrorgroupid=<groupID>    ID of the group that is to be removed.\n"
      "  --nodetype=<type>            type must be `storage'. Buddy group removal is\n"
      "                               not yet supported for meta buddy groups.\n"
      "\n"
      " Optional:\n"
      "  --dry-run                    Only check whether the group could safely be\n"
      "                               removed, but do not actually remove the group.\n"
      "\n"
      "USAGE:\n"
      " To determine whether storage buddy group 7 still contains active data:\n"
      "   $ beegfs-ctl --removemirrorgroup --mirrorgroupid=7 --nodetype=storage \\\n"
      "        --dry-run\n"
      "\n"
      " To actually remove storage buddy group 7:\n"
      "   $ beegfs-ctl --removemirrorgroup --mirrorgroupid=7 --nodetype=storage\n\n"
      "WARNING: Incorrect use of this command can corrupt your system. Please\n"
      "make sure that one of the following conditions is met before removing a group:\n\n"
      "* The filesystem is completely empty, with no files and no disposal files.\n\n"
      "* The filesystem is offline (no mounted clients) and the last operations \n"
      "  done with the filesystem were:\n"
      "   * Migrate everything off of the group you want to remove.\n"
      "   * Check whether disposal is empty (if not, abort).\n"
      "   * Unmount the client used for migration.\n";

   std::cout << std::flush;
}

FhgfsOpsErr ModeRemoveBuddyGroup::removeGroup(const uint16_t groupID, const bool dryRun,
      const bool force)
{
   RemoveBuddyGroupMsg msg(NODETYPE_Storage, groupID, dryRun, force);

   auto node = Program::getApp()->getMgmtNodes()->referenceFirstNode();

   const auto resultMsg = MessagingTk::requestResponse(*node, msg, NETMSGTYPE_RemoveBuddyGroupResp);
   if (!resultMsg)
      return FhgfsOpsErr_COMMUNICATION;

   return static_cast<RemoveBuddyGroupRespMsg&>(*resultMsg).getResult();
}
