#ifndef OPEN_FHGFSTYPES_H_
#define OPEN_FHGFSTYPES_H_

#include <linux/in.h>
#include <linux/time.h>

struct fhgfs_sockaddr_in
{
   struct in_addr addr;
   __be16 port;
};
typedef struct fhgfs_sockaddr_in fhgfs_sockaddr_in;



struct fhgfs_stat
{
   umode_t mode;
   unsigned int nlink;
   uid_t uid;
   gid_t gid;
   loff_t size;
   uint64_t blocks;
   struct timespec atime;
   struct timespec mtime;
   struct timespec ctime; // attrib change time (not creation time)
};
typedef struct fhgfs_stat fhgfs_stat;



#endif /* OPEN_FHGFSTYPES_H_ */
