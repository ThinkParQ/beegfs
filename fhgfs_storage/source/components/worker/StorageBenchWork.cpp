#include <common/app/log/LogContext.h>
#include <common/benchmark/StorageBench.h>
#include <common/toolkit/StringTk.h>
#include <program/Program.h>
#include "StorageBenchWork.h"

void StorageBenchWork::process(char* bufIn, unsigned bufInLen, char* bufOut,
   unsigned bufOutLen)
{
   const char* logContext = "Storage Benchmark (run)";

   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   int workRes = 0; // return value for benchmark operator
   ssize_t ioRes = 0; // read/write result

   if (this->type == StorageBenchType_READ)
   {
      size_t readSize = cfg->getTuneFileReadSize();
      size_t toBeRead = this->bufLen;
      size_t bufOffset = 0;

      while(toBeRead)
      {
         size_t currentReadSize = BEEGFS_MIN(readSize, toBeRead);

         ioRes = read(this->fileDescriptor, &this->buf[bufOffset], currentReadSize);
         if (ioRes <= 0)
            break;

         toBeRead -= currentReadSize;
         bufOffset += currentReadSize;
      }

      app->getNodeOpStats()->updateNodeOp(0, StorageOpCounter_READOPS,
         this->bufLen, NETMSG_DEFAULT_USERID);
   }
   else
   if (this->type == StorageBenchType_WRITE)
   {
      size_t writeSize = cfg->getTuneFileWriteSize();
      size_t toBeWritten = this->bufLen;
      size_t bufOffset = 0;

      while(toBeWritten)
      {
         size_t currentWriteSize = BEEGFS_MIN(writeSize, toBeWritten);

         ioRes = write(this->fileDescriptor, &this->buf[bufOffset], currentWriteSize);
         if (ioRes <= 0)
            break;

         toBeWritten -= currentWriteSize;
         bufOffset += currentWriteSize;
      }

      app->getNodeOpStats()->updateNodeOp(0, StorageOpCounter_WRITEOPS,
         this->bufLen, NETMSG_DEFAULT_USERID);
   }
   else
   { // unknown benchmark type
      workRes = STORAGEBENCH_ERROR_WORKER_ERROR;
      LogContext(logContext).logErr("Error: unknown benchmark type");
   }

   if(unlikely(workRes < 0) || unlikely(ioRes == -1) )
   { // error occurred
      if (ioRes == -1)
      { // read or write operation failed
         LogContext(logContext).logErr(std::string("Error: I/O failure. SysErr: ") +
            System::getErrString() );
      }

      workRes = STORAGEBENCH_ERROR_WORKER_ERROR;

      this->operatorCommunication->getWriteFD()->write(&workRes, sizeof(int) );
   }
   else
   { // success
      this->operatorCommunication->getWriteFD()->write(&this->threadID, sizeof(int) );
   }
}

