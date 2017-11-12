#!/usr/bin/python


import fileinput
import getopt
import os
import sys
import shutil


FHGFS_CONF_DIR = os.path.normpath('/etc/fhgfs')
FHGFS_LIB_PATH = os.path.normpath('/etc/fhgfs/lib')
BEEGFS_CONF_DIR = os.path.normpath('/etc/beegfs')
BEEGFS_LIB_PATH = os.path.normpath('/etc/beegfs/lib')
DEFAULT_DIR = os.path.normpath('/etc/default')



def copyConfigDir(verbose):
   for root, dirs, files in os.walk(FHGFS_CONF_DIR):
      for dirname in dirs:
         sourceDirPath = os.path.normpath(os.path.join(root, dirname) )
         destDirPath = replaceFhgfsByBeeGFS(sourceDirPath)

         if sourceDirPath.startswith(FHGFS_LIB_PATH):
            if verbose:
               message = "skip directory: " + sourceDirPath
               print >> sys.stdout, message
            continue

         if verbose:
            message = "create directory: " + destDirPath
            print >> sys.stdout, message

         os.mkdir(destDirPath, 0o755)

      for filename in files:
         sourceFilePath = os.path.normpath(os.path.join(root, filename) )
         destFilePath = replaceFhgfsByBeeGFS(sourceFilePath)

         if sourceFilePath.startswith(FHGFS_LIB_PATH):
            if verbose:
               message = "skip file: " + sourceFilePath
               print >> sys.stdout, message
            continue

         if verbose:
            message = "copy file: " + sourceFilePath + " to " + destFilePath
            print >> sys.stdout, message

         shutil.copy2(sourceFilePath, destFilePath)
         
         if filename == "fhgfs-mounts.conf":
            parseMountConfig(destFilePath, verbose)
         else:
            parseConfigFiles(destFilePath, verbose)


def copyDefaultFiles(verbose):
   dirs = os.listdir(DEFAULT_DIR)
   for filename in dirs:
      sourceFilePath = os.path.normpath(os.path.join(DEFAULT_DIR, filename) )
      if not filename.startswith("fhgfs"):
         if verbose:
            message = "skip file: " + sourceFilePath
            print >> sys.stdout, message
         continue

      if os.path.isdir(sourceFilePath):
         if verbose:
            message = "skip directory: " + sourceFilePath
            print >> sys.stdout, message
         continue

      destFilePath = replaceFhgfsByBeeGFS(sourceFilePath)
      if verbose:
         message = "copy file: " + sourceFilePath + " to " + destFilePath
         print >> sys.stdout, message

      shutil.copy2(sourceFilePath, destFilePath)


def parseConfigFiles(file, verbose):
   if not os.path.exists(file):
      message = "configuration file doesn't exists: " + file
      print >> sys.stdout, message
      sys.exit(1)

   if verbose:
      message = "parse file: " + file
      print >> sys.stdout, message

   for line in fileinput.input(file, inplace=True):
      line.strip()
      if line.startswith("#"):
         if os.path.basename(file) == "beegfs-client-autobuild.conf":
            sys.stdout.write(line.replace("FHGFS_OPENTK_IBVERBS", "BEEGFS_OPENTK_IBVERBS") )
         else:
            sys.stdout.write(line)
      elif line.startswith("connInterfacesFile"):
         lineSplit = line.split("=")
         if lineSplit[1].strip().startswith(FHGFS_CONF_DIR):
            sys.stdout.write(replaceFhgfsByBeeGFS(line) )
         else:
            sys.stdout.write(line)
      elif line.startswith("connNetFilterFile"):
         lineSplit = line.split("=")
         if lineSplit[1].strip().startswith(FHGFS_CONF_DIR):
            sys.stdout.write(replaceFhgfsByBeeGFS(line) )
         else:
            sys.stdout.write(line)
      elif line.startswith("connTcpOnlyFilterFile"):
         lineSplit = line.split("=")
         if lineSplit[1].strip().startswith(FHGFS_CONF_DIR):
            sys.stdout.write(replaceFhgfsByBeeGFS(line) )
         else:
            sys.stdout.write(line)
      elif line.startswith("logStdFile"):
         sys.stdout.write(replaceFhgfsByBeeGFS(line) )
      elif line.startswith("databaseFile"):
         sys.stdout.write(replaceFhgfsByBeeGFS(line) )
      elif line.startswith("buildArgs"):
         sys.stdout.write(line.replace("FHGFS_OPENTK_IBVERBS", "BEEGFS_OPENTK_IBVERBS") )
      else:
         sys.stdout.write(line)

   fileinput.close()


def parseMountConfig(destFilePath, verbose):
   if not os.path.exists(destFilePath):
      message = "configuration file doesn't exists: " + destFilePath
      print >> sys.stdout, message
      sys.exit(1)

   if verbose:
      message = "parse file: " + destFilePath
      print >> sys.stdout, message

   for line in fileinput.input(destFilePath, inplace=True):
      line.strip()
      if line.startswith("#"):
         sys.stdout.write(line)
      else:
         lineSplit = line.split(" ")
         confFile = replaceFhgfsByBeeGFS(lineSplit[1].strip() )
         sys.stdout.write(lineSplit[0] + " " + confFile + "\n")

   fileinput.close()


def replaceFhgfsByBeeGFS(inString):
   outString = inString.replace("FHGFS", "BEEGFS")
   outString = outString.replace("fhgfs", "beegfs")
   return outString.replace("FhGFS", "BeeGFS")


def process(verbose) :
   if not os.path.exists(FHGFS_CONF_DIR):
      message = "\nCan't find FhGFS configuration directory: " + FHGFS_CONF_DIR
      print >> sys.stdout, message
      sys.exit(1)

   if not os.path.exists(BEEGFS_CONF_DIR):
      os.mkdir(BEEGFS_CONF_DIR, 0o755)

   message = "\nCopy all configuration files from " + FHGFS_CONF_DIR +" to " + BEEGFS_CONF_DIR + \
   " and rename to beegfs-...\n"
   print >> sys.stdout, message
   copyConfigDir(verbose)

   message = "\nCopy all FhGFS default files " + DEFAULT_DIR + " to BeeGFS default files.\n"
   print >> sys.stdout, message
   copyDefaultFiles(verbose)


def processArgs(programName, opts):
   verbose = False
   for opt, arg in opts:
      if opt == "--verbose":
         verbose = True
      else:
         printHelp(programName)
         
   process(verbose)


def printHelp(programName):
   message =  "Description:\n"
   message += " This tool migrates (i.e. copies and modifies) the configuration files of FhGFS\n"
   message += " version 2014.01 to BeeGFS version 2015.03 configuration files.\n"
   message += "\n"
   message += " The following tasks will be performed:\n"
   message += " * Migrate all configuration files from " + FHGFS_CONF_DIR + " to " + BEEGFS_CONF_DIR + "\n"
   message += "   * Update 'fhgfs' within the config file path settings of these options:\n"
   message += "     connInterfacesFile, connNetFilterFile, logStdFile and databaseFile.\n"
   message += " * Update the name of the client module autobuild argument for RDMA support.\n"
   message += " * Update all fhgfs-* service startup configuration files in " + DEFAULT_DIR + ".\n"
   message += "\n"
   message += "Note:\n"
   message += " * The config file options storeMgmtdDirectory, storeMetaDirectory and\n"
   message += "   storeStorageDirectory are not changed, even if the path contains 'fhgfs'.\n"
   message += "   * This means you don't have to move your existing data.\n"
   message += " * Run this tool on all servers and clients.\n"
   message += "\n"
   message += "Usage:\n"
   message += " # " + str(programName) + " [options]\n"
   message += "\n"
   message += "Options:\n"
   message += " --help                 Prints this text.\n"
   message += " --verbose              Explain what is being done.\n"

   print >> sys.stdout, message
   sys.exit(1);


def main():
   programName = os.path.basename(sys.argv[0])

   try:
      opts, args = getopt.getopt(sys.argv[1:], "h", ["verbose", "help"])

      processArgs(programName, opts)

   except getopt.error, msg:
      print msg
      print "For help use '--help'"
      sys.exit(1)

main()
