#include <common/threading/Mutex.h>
#include <common/toolkit/StringTk.h>
#include "SecurityTk.h"

#include <mutex>

std::map<int,int64_t> SecurityTk::nonces;
Mutex SecurityTk::nonceMutex;

std::string SecurityTk::DoMD5(std::string str)
{
   unsigned char out[33];
   char hex_output[33];
   char* in = strdup(str.c_str());
   MD5((unsigned char*) in, str.length(), out);
   for (int i = 0; i < 16; ++i)
   {
      sprintf(hex_output + i * 2, "%02x", out[i]);
   }
   SAFE_FREE(in);
   return std::string(hex_output);
}

std::string SecurityTk::DoSHA(std::string str1)
{
   unsigned long int h0, h1, h2, h3, h4, a, b, c, d, e, f, k, temp;

   h0 = 0x67452301;
   h1 = 0xEFCDAB89;
   h2 = 0x98BADCFE;
   h3 = 0x10325476;
   h4 = 0xC3D2E1F0;

   unsigned char *str;
   str = (unsigned char *) malloc(strlen(str1.c_str()) + 100);
   strcpy((char *) str, str1.c_str()); // NOLINT

   int current_length = strlen((const char *) str);
   int original_length = current_length;
   str[current_length] = 0x80;
   str[current_length + 1] = '\0';

   current_length++;

   int ib = current_length % 64;
   if (ib < 56)
      ib = 56 - ib;
   else
      ib = 120 - ib;

   for (int i = 0; i < ib; i++)
   {
      str[current_length] = 0x00;
      current_length++;
   }
   str[current_length + 1] = '\0';

   for (int i = 0; i < 6; i++)
   {
      str[current_length] = 0x0;
      current_length++;
   }

   str[current_length] = (original_length * 8) / 0x100;
   current_length++;
   str[current_length] = (original_length * 8) % 0x100;
   current_length++;
   str[current_length + 1] = '\0';

   int number_of_chunks = current_length / 64;
   unsigned long int word[80];
   for (int i = 0; i < number_of_chunks; i++)
   {
      for (int j = 0; j < 16; j++)
      {
         word[j] = str[i * 64 + j * 4 + 0] * 0x1000000 + str[i * 64 + j * 4 + 1] * 0x10000 + str[i
            * 64 + j * 4 + 2] * 0x100 + str[i * 64 + j * 4 + 3];
      }

      for (int j = 16; j < 80; j++)
      {
         word[j] = rotateleft((word[j-3] ^ word[j-8] ^ word[j-14] ^ word[j-16]),1);
      }

      a = h0;
      b = h1;
      c = h2;
      d = h3;
      e = h4;

      for (int m = 0; m < 80; m++)
      {
         if (m <= 19)
         {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
         }
         else if (m <= 39)
         {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
         }

         else if (m <= 59)
         {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
         }

         else
         {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
         }

         temp = (rotateleft(a,5) + f + e + k + word[m]) & 0xFFFFFFFF;
         e = d;
         d = c;
         c = rotateleft(b,30);
         b = a;
         a = temp;

      }

      h0 = h0 + a;
      h1 = h1 + b;
      h2 = h2 + c;
      h3 = h3 + d;
      h4 = h4 + e;

   }

   char buf[40];
   sprintf(buf, "%lx%lx%lx%lx%lx", h0, h1, h2, h3, h4);
   std::string outStr(buf);
   free(str);
   return outStr;
}


std::string SecurityTk::str_AND(std::string str1, std::string str2)
{
    std::string retval(str1);

    short unsigned int a_len=str1.length();
    short unsigned int b_len=str2.length();
    short unsigned int i=0;
    short unsigned int j=0;

    for(i=0;i<a_len;i++)
    {
        retval[i]=str1[i] & str2[i];
        j=(++j<b_len?j:0);
    }

    return retval;
}

int SecurityTk::createNonce(int64_t *outNonce)
{
   /* initialize random seed: */
   srand ( time(NULL) );
   int64_t intNonce = rand();
   int id = 0;

   const std::lock_guard<Mutex> lock(nonceMutex);

   std::map<int,int64_t>::iterator iter = SecurityTk::nonces.find(id);
   while (iter != SecurityTk::nonces.end())
   {
      id = rand() % 10000;
      iter = SecurityTk::nonces.find(id);
   }
   SecurityTk::nonces[id] = intNonce;
   *outNonce = intNonce;

   return id;
}

void SecurityTk::clearNonceLocked(int nonceID)
{
   const std::lock_guard<Mutex> lock(nonceMutex);
   SecurityTk::nonces.erase(nonceID);
}

void SecurityTk::clearNonce(int nonceID)
{
   SecurityTk::nonces.erase(nonceID);
}

bool SecurityTk::checkReply(std::string reply, std::string secret, int nonceID)
{
   const std::lock_guard<Mutex> lock(nonceMutex);

   std::string expected = SecurityTk::DoMD5(secret + StringTk::int64ToStr(SecurityTk::nonces[nonceID] ) );
   SecurityTk::clearNonce(nonceID);

   return (reply.compare(expected) == 0);
}

std::string SecurityTk::cryptWithNonce(std::string str, int nonceID)
{
   const std::lock_guard<Mutex> lock(nonceMutex);

   std::string crypted = SecurityTk::DoMD5(str + StringTk::int64ToStr(SecurityTk::nonces[nonceID] ) );
   SecurityTk::clearNonce(nonceID);

   return crypted;
}

void SecurityTk::cryptWithNonce(StringList *inList, StringList *outList, int nonceID)
{
   const std::lock_guard<Mutex> lock(nonceMutex);

   for (StringListIter iter = inList->begin(); iter != inList->end(); iter++)
   {
      std::string str = *iter;
      std::string crypted = SecurityTk::DoMD5(str + StringTk::int64ToStr(SecurityTk::nonces[nonceID] ) );
      outList->push_back(crypted);
   }
   SecurityTk::clearNonce(nonceID);
}
