#ifndef TOOLKIT_SECURITYTK_H_
#define TOOLKIT_SECURITYTK_H_

// small class with static functions to apply cryptographic or hashing functions to strings
// also supports nonce handling for the communication

#include <common/Common.h>
#include <common/threading/Mutex.h>

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <openssl/md5.h>

#define rotateleft(x,n) ((x<<n) | (x>>(32-n)))
#define rotateright(x,n) ((x>>n) | (x<<(32-n)))


class SecurityTk
{
   public:
      static std::string DoSHA(std::string str1);
      static std::string DoMD5(std::string str);

      static int createNonce(int64_t *outNonce);
      static void clearNonce(int nonceID);
      static void clearNonceLocked(int nonceID);
      static bool checkReply(std::string reply, std::string secret, int nonceID);
      static std::string cryptWithNonce(std::string str, int nonceID);
      static void cryptWithNonce(StringList *inList, StringList *outList, int nonceID);


   private:
      SecurityTk() {};
      static Mutex nonceMutex;
      static std::map<int, int64_t> nonces;
      static std::string str_AND(std::string str1, std::string str2);
};

#endif /*TOOLKIT_SECURITYTK_H_*/
