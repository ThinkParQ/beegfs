#include "com_beegfs_JBeeGFS.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>

#include <beegfs/beegfs.h>

JNIEXPORT jint JNICALL Java_com_beegfs_JBeeGFS_open(JNIEnv* env, jobject obj, jstring jFileName) {
   const char* fileName = env->GetStringUTFChars(jFileName, 0);
   int fd = open(fileName, O_RDONLY);
   if (fd == -1) {
      const unsigned errbufsize = 128;
      switch (errno) {
         case ELOOP: {
            jclass exceptionClass = env->FindClass("java/io/IOException");
            env->ThrowNew(exceptionClass, fileName);
         }
         break;

         // interrupted
         case EINTR: {
            jclass exceptionClass = env->FindClass("java/io/InterruptedIOException");
            env->ThrowNew(exceptionClass, "open() interrupted.");
         }
         break;

         // Problem with special files/ defice files
         case ENODEV:
         case ENXIO:
         // out of (any kind of) memory, name too long etc.
         case EMFILE:
         case ENAMETOOLONG:
         case ENFILE:
         case ENOMEM:
         case EFBIG:
         case EOVERFLOW: {
            char errbuf[errbufsize];
            const char* errstr = strerror_r(errno, &errbuf[0], errbufsize);
            jstring jErrorString = env->NewStringUTF(errstr);
            jclass jError_class = env->FindClass("java/io/IOException");
            jmethodID jError_constructor = env->GetMethodID(jError_class, "<init>",
                  "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
            jthrowable jError = static_cast<jthrowable>(
                  env->NewObject(jError_class, jError_constructor, jFileName, NULL,
                  jErrorString) );
            env->Throw(jError);
            break;
         }

         // File not found
         case ENOTDIR:
         case EFAULT:
         case ENOENT: {
            jclass exceptionClass = env->FindClass("java/io/FileNotFoundException");
            env->ThrowNew(exceptionClass, "open(): File not found.");
            break;
         }

         // Permission denied.
         case EACCES:
         case EPERM: {
            char errbuf[errbufsize];
            const char* errstr = strerror_r(errno, &errbuf[0], errbufsize);
            jstring jErrorString = env->NewStringUTF(errstr);
            jclass jError_class = env->FindClass("java/io/IOException");
            jmethodID jError_constructor = env->GetMethodID(jError_class, "<init>",
                  "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
            jthrowable jError = static_cast<jthrowable>(
                  env->NewObject(jError_class, jError_constructor, jFileName, NULL,
                  jErrorString) );
            env->Throw(jError);
            break;
         }

         // Those are 'impossible' in our case, e.g. can only occur when writing or making new files
         case EEXIST:
         case EISDIR:
         case ENOSPC:
         case EROFS:
         case ETXTBSY:
         case EWOULDBLOCK:
         // fallthrough

         default: {
            char errbuf[errbufsize];
            const char* errstr = strerror_r(errno, &errbuf[0], errbufsize);
            jclass exceptionClass = env->FindClass("java/lang/Error");
            env->ThrowNew(exceptionClass, errstr);
            break;
         }
      }
   }

   return static_cast<jint>(fd);
}

/**
 * Helper function to access the member 'nativeFileDescriptor' in the Java 'JBeeGFS' object.
 */
int getFd(JNIEnv* env, jobject obj) {
   jclass jJBeeGFS_class = env->GetObjectClass(obj);
   jfieldID jNativeFileDescriptorFID = env->GetFieldID(jJBeeGFS_class, "nativeFileDescriptor", "I");

   jint jNativeFileDescriptor = env->GetIntField(obj, jNativeFileDescriptorFID);
   return static_cast<int>(jNativeFileDescriptor);
}

JNIEXPORT void JNICALL Java_com_beegfs_JBeeGFS_close(JNIEnv* env, jobject obj) {
   int ret = close(getFd(env, obj) );
   if (ret != 0) {
      switch (errno) {
         case EBADF: {
            jclass exceptionClass = env->FindClass("java/io/FileNotFoundException");
            env->ThrowNew(exceptionClass, "close(): not a valid open file descriptor");
            break;
         }
         case EINTR: {
            jclass exceptionClass = env->FindClass("java/io/InterruptedIOException");
            env->ThrowNew(exceptionClass, "close() interrupted.");
            break; 
         }
         case EIO: {
            jclass jError_class = env->FindClass("java/io/IOError");
            jmethodID jError_constructor = env->GetMethodID(
                  jError_class, "<init>", "(Ljava/lang/Throwable;)V");
            jthrowable jError = static_cast<jthrowable>(
                  env->NewObject(jError_class, jError_constructor, NULL) );
            env->Throw(jError);
            break;
         }
         default: {
            jclass exceptionClass = env->FindClass("java/lang/Error");
            env->ThrowNew(exceptionClass, "close(): Error.");
            break;
         }
      }
   }
}

JNIEXPORT jboolean JNICALL Java_com_beegfs_JBeeGFS_isBeegFS(JNIEnv* env, jobject obj) {
   return beegfs_testIsBeeGFS(getFd(env, obj) );
}

JNIEXPORT jstring JNICALL Java_com_beegfs_JBeeGFS_getConfigPath(JNIEnv* env, jobject obj) {
   char* configPath;
   bool res = beegfs_getConfigFile(getFd(env, obj), &configPath);
   if (res) {
      jstring jret = env->NewStringUTF(configPath);
      free(configPath);
      return jret;
   } else {
      return NULL;
   }
}

JNIEXPORT jstring JNICALL Java_com_beegfs_JBeeGFS_getRuntimeConfigPath(JNIEnv* env, jobject obj) {
   char* runtimeConfigPath;
   bool res = beegfs_getRuntimeConfigFile(getFd(env, obj), &runtimeConfigPath);
   if (res) {
      jstring jret = env->NewStringUTF(runtimeConfigPath);
      free(runtimeConfigPath);
      return jret;
   } else {
      return NULL;
   }
}

JNIEXPORT jstring JNICALL Java_com_beegfs_JBeeGFS_getMountID(JNIEnv* env, jobject obj) {
   char* mountID;
   bool res = beegfs_getMountID(getFd(env, obj), &mountID);
   if (res) {
      jstring jret = env->NewStringUTF(mountID);
      free(mountID);
      return jret;
   } else {
      return NULL;
   }
}

JNIEXPORT jobject JNICALL Java_com_beegfs_JBeeGFS_getStripeInfo(JNIEnv* env, jobject obj) {
   unsigned patternType;
   unsigned chunkSize;
   uint16_t numTargets;
   if (beegfs_getStripeInfo(getFd(env, obj), &patternType, &chunkSize, &numTargets) ) {
      jclass jstripeinfo = env->FindClass("com/beegfs/JBeeGFS$StripeInfo");
      if (!jstripeinfo) return NULL;
      jmethodID jstripeinfoconstructor = env->GetMethodID(jstripeinfo, "<init>", "(IJI)V");
      if (!jstripeinfoconstructor) return NULL;
      jobject jstripeinfoobject = env->NewObject(jstripeinfo, jstripeinfoconstructor,
            patternType, chunkSize, numTargets);

      return jstripeinfoobject;
   } else {
      return NULL;
   }
}

JNIEXPORT jobject JNICALL Java_com_beegfs_JBeeGFS_getStripeTarget(JNIEnv* env, jobject obj,
      jint jTargetIndex) {
   int targetIndex = jTargetIndex;
   const int fd = getFd(env, obj);

   uint16_t targetNumID, nodeNumID;
   char* nodeName;
   if (beegfs_getStripeTarget(fd, targetIndex, &targetNumID, &nodeNumID, &nodeName) ) {
      jclass jStripeTarget_class = env->FindClass("com/beegfs/JBeeGFS$StripeTarget");
      if (!jStripeTarget_class) return NULL;

      jmethodID jStripeTarget_constructor = env->GetMethodID(jStripeTarget_class, "<init>",
            "(IILjava/lang/String;)V");
      if (!jStripeTarget_constructor) return NULL;

      jstring jNodeName = env->NewStringUTF(nodeName);
      jobject jStripeTarget = env->NewObject(jStripeTarget_class, jStripeTarget_constructor,
            targetNumID, nodeNumID, jNodeName);

      return jStripeTarget;
   } else {
      return NULL;
   }
}
