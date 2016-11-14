#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <stdlib.h>

#include <deeper_cache.h>



static const mode_t MODE_FLAG = S_IRWXU | S_IRGRP | S_IROTH;
static const int OPEN_FLAGS = O_CREAT | O_RDWR;
static const int DATA_SIZE = 1024 * 1024 * 25;


/**
 * Writes random data to a file descriptor
 *
 * @param fd the file descriptor of the file to write the data
 * @param amountOfDataBytes the amount of data to write into the file in bytes
 * @return number of bytes written to the file
 */
int copyRandomDataToFile(int fd, size_t amountOfDataBytes)
{
   size_t sizeWritten = 0;

   if( (amountOfDataBytes > 0) && (fd > 0) )
   {
      struct timeval tv;
      gettimeofday(&tv, NULL);
      unsigned seed = tv.tv_usec;

      char* buf = (char*) malloc(amountOfDataBytes);

      for(size_t bs=0; bs < amountOfDataBytes; bs++)
         buf[bs] = rand_r(&seed);

      sizeWritten = write(fd, buf, amountOfDataBytes);
      if(sizeWritten != amountOfDataBytes)
         std::cout << __FUNCTION__ << ": can not write data to FD: " << fd << " errno: "
            << errno << std::endl;

      free(buf);
   }

   return sizeWritten;
}



int main(int argc, char** argv)
{
   if(argc != 2)
   {
      std::cout << "Usage: " << argv[0] << " $MOUNT_GLOBAL_BEEGFS" << std::endl;
      exit(-1);
   }

   std::string globalFS(argv[1]);
   std::string dir(globalFS + "/dir");
   std::string file(dir + "/file");



   // create a directory on global FS with POSIX syscalls
   int funcError = mkdir(dir.c_str(), MODE_FLAG);
   if(funcError)
   {
      std::cout << "mkdir: can not create directory: " << dir << " errno: " << errno
         << std::endl;
      exit(-1);
   }

   // create a file on global FS with POSIX syscalls
   int globalFD = open(file.c_str(),OPEN_FLAGS , MODE_FLAG);
   if(globalFD == -1)
   {
      std::cout << "open: can not create file: " << file << " errno: " << errno << std::endl;
      exit(-1);
   }

   // write some data to the file on the global FS
   copyRandomDataToFile(globalFD, DATA_SIZE);
   close(globalFD);



   // create the directory on cache FS
   funcError = deeper_cache_mkdir(dir.c_str(), MODE_FLAG);
   if(funcError == DEEPER_RETVAL_ERROR)
   {
      std::cout << "deeper_cache_mkdir: can not create directory: " << dir << " errno: " << errno
         << std::endl;
      exit(-1);
   }

   // prefetch the file from global FS to the cache FS
   funcError = deeper_cache_prefetch(file.c_str(), DEEPER_PREFETCH_NONE);
   if(funcError == DEEPER_RETVAL_ERROR)
   {
      std::cout << "deeper_cache_prefetch: can not prefetch file: " << file << " errno: " << errno
         << std::endl;
      exit(-1);
   }

   // wait until the prefetch is finished
   funcError = deeper_cache_prefetch_wait(file.c_str(), DEEPER_PREFETCH_NONE);
   if(funcError == DEEPER_RETVAL_ERROR)
   {
      std::cout << "deeper_cache_prefetch_wait: can not wait for prefetch of file: " << file
         << " errno: " << errno << std::endl;
      exit(-1);
   }

   // open file in the cache FS
   int cacheFD = deeper_cache_open(file.c_str(), O_RDWR, MODE_FLAG, DEEPER_OPEN_NONE);
   if(cacheFD == -1)
   {
      std::cout << "deeper_cache_open: can not open file: " << file << " errno: " << errno
         << std::endl;
      exit(-1);
   }

   // modify the file in the cache FS
   copyRandomDataToFile(cacheFD, 2 * DATA_SIZE);

   // close file in the cache FS
   funcError = deeper_cache_close(cacheFD);
   if(funcError == DEEPER_RETVAL_ERROR)
   {
      std::cout << "deeper_cache_close: can not close file: " << file << " errno: " << errno
         << std::endl;
      exit(-1);
   }

   // flush the file from the cache FS to the global FS and wait until the flush is finished
   funcError = deeper_cache_flush(file.c_str(), DEEPER_FLUSH_WAIT);
   if(funcError == DEEPER_RETVAL_ERROR)
   {
      std::cout << "deeper_cache_flush: can not flush (DEEPER_FLUSH_WAIT) file: " << file
         << " errno: " << errno << std::endl;
      exit(-1);
   }
}
