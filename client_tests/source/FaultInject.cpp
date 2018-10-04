#include "FaultInject.h"
#include <fstream>
#include <iostream>

static const std::string& getDebugFSMount()
{
   static std::string debugFSMount;

   if(!debugFSMount.empty() )
      return debugFSMount;

   std::ifstream mounts("/proc/mounts");

   if(!mounts)
      throw std::runtime_error("could not open /proc/mounts to locate debugfs");

   while(mounts && !mounts.eof() )
   {
      std::string device, mount, fsType;

      std::getline(mounts, device, ' ');
      std::getline(mounts, mount, ' ');
      std::getline(mounts, fsType, ' ');

      if(fsType == "debugfs")
      {
         debugFSMount = mount;
         return debugFSMount;
      }

      std::getline(mounts, mount);
   }

   throw std::runtime_error("could not locate debugfs, not mounted?");
}


static void writeFileAt(const SafeFD& dir, const std::string& file, const std::string& data)
{
   auto fd = SafeFD::openAt(dir, file, O_WRONLY);

   auto writeRes = ::write(fd.raw(), data.c_str(), data.size() );
   if(writeRes != (int) data.size() )
      throw std::runtime_error("could not write " + file);
}

InjectFault::InjectFault(const char* type, bool quiet)
   : faultDir(SafeFD::open(getDebugFSMount() + "/beegfs/fault/" + type, O_DIRECTORY) )
{
   writeFileAt(this->faultDir, "interval", "1");
   writeFileAt(this->faultDir, "probability", "100");
   writeFileAt(this->faultDir, "task-filter", "N");
   writeFileAt(this->faultDir, "times", "-1");
   writeFileAt(this->faultDir, "verbose", quiet ? "1" : "2");
}

InjectFault::~InjectFault()
{
   writeFileAt(this->faultDir, "task-filter", "Y");
}
