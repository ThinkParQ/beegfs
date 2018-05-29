package com.beegfs;

import java.io.InterruptedIOException;
import java.io.IOError;
import java.io.IOException;
import java.io.FileNotFoundException;

/**
 * Wrapper class for the calls to the beegfs ioctl library.
 */
public class JBeeGFS {
   static {
      System.loadLibrary("jbeegfs");
   }

   static final private int JBEEGFS_API_MAJOR_VERSION = 1; // major version number of the API,
                                                           // different major version are 
                                                           // incompatible
   static final private int JBEEGFS_API_MINOR_VERSION = 0; // minor version number of the API, the
                                                           // minor versions of the same major
                                                           // version are backward compatible

   final private int nativeFileDescriptor;

   /**
    * Class to store stripe info (pattern type, chunk size and number of targets) in.
    */
   static class StripeInfo {
      private int patternType;
      private long chunkSize;
      private int numTargets;

      public StripeInfo(int patternType, long chunkSize, int numTargets) {
         this.patternType = patternType;
         this.chunkSize = chunkSize;
         this.numTargets = numTargets;
      }

      public int getPatternType() { return patternType; }
      public long getChunkSize() { return chunkSize; }
      public int getNumTargets() { return numTargets; }
   }

   /**
    * This class stores information about a storage target: The target's numerical ID, the node's
    * numerical ID, and the hostname the storage target is on.
    */
   static class StripeTarget {
      private int targetNumID;
      private int nodeNumID;
      private String nodeName;

      public StripeTarget(int targetNumID, int nodeNumID, String nodeName) {
         this.targetNumID = targetNumID;
         this.nodeNumID = nodeNumID;
         this.nodeName = nodeName;
      }

      int getTargetNumID() { return targetNumID; }
      int getNodeNumID() { return nodeNumID; }
      String getNodeName() { return nodeName; }
   }

   /**
    * Constructs a new JBeeGFS object for a file. The file is open()ed during construction of this
    * object, and closed via the close() method.
    * @param fileName the name of the file.
    * @throws IOException see {@link open(String fileName)} for list of exceptions
    * @throws InterruptedIOException see {@link open(String fileName)} for list of exceptions
    */
   public JBeeGFS(String fileName) throws IOException, InterruptedIOException {
      nativeFileDescriptor = open(fileName);
   }

   /**
    * Checks if the required API version of the application is compatible to current API version
    *
    * @param requiredMajorVersion the required major API version of the user application
    * @param requiredMinorVersion the minimal required minor API version of the user application
    * @return 0 if the required version and the API version are compatible, if not -1 is returned
    */
   public static boolean jBeegfsCheckApiVersion(int requiredMajorVersion, int requiredMinorVersion) {
      if(requiredMajorVersion != JBEEGFS_API_MAJOR_VERSION)
         return false;

      if(requiredMinorVersion > JBEEGFS_API_MINOR_VERSION)
         return false;

      return true;
   }

   /**
    * Calls the native open() to get a file descriptor for a file.
    * @param fileName the path to the file.
    * @throws InterruptedIOException if the native call was interrupted.
    * @throws IOException if too many symbolic links were encountered in resolving the
    *       path, or if the user does not have access rights to the file, or if path to a device
    *       file was given but no corresponding device exists; if the maximum number of file
    *       descriptors (for the process or global) is
    *       exceeded; if the path name is too long; if a file is too large to be opened (file size
    *       over 4GiB on a 32 bit system). (see getReason() for more information)
    * @throws FileNotFoundException if the file does not exist, or component used as a direcotry in
    *       the path is actually a file.
    * @throws Error if something else goes wrong - might indicate a bug in the library.
    * @return the native file descrptor to the opened file.
    */
   private native int open(String fileName) throws FileNotFoundException, InterruptedIOException,
         IOException, Error;

   /**
    * Closes the file discriptor stored in {@link nativeFileDescriptor}.
    * @throws FileNotFoundException if fd is not a valid open file descriptor.
    * @throws InterruptedIOException if native close() call was interrupted by a signal.
    * @throws IOError if an I/O error occured.
    */
   public native void close() throws FileNotFoundException, InterruptedIOException, IOError, Error;

   /**
    * @return the native file descriptor of the file opened by this object.
    */
   public int getNativeFileDescriptor() { return nativeFileDescriptor; }


   /**
    * @return whether the location is on a BeeGFS mount.
    */
   public native boolean isBeegFS();

   /**
    * @return the path to the BeeGFS config file, null in case of an error.
    */
   public native String getConfigPath();

   /**
    * @return the path to the beegfs-clien't's runtime config, null in case of an error.
    */
   public native String getRuntimeConfigPath();

   /**
    * @return the mountID aka clientID aka nodeID of client mount aka sessionID, null in case of an
    *       error.
    */
   public native String getMountID();

   /**
    * @return a StripeInfo object which holds the pattern type, chunk size and number of targets
    *       for the file. In case of an error, null is returned.
    */
   public native StripeInfo getStripeInfo();

   /**
    * @param targetIndex th index of the target. Must be in [0 .. StripeInfo.numTargets].
    * @return a StripeTarged object holding information about a storage target a file is stored on.
    *       in case of an error or invalid index, null is returned.
    */
   public native StripeTarget getStripeTarget(int targetIndex);
}
