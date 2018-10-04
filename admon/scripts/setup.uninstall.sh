#!/bin/bash

dirname=$(dirname $0)

. ${dirname}/setup.defaults


uninstall_package()
{
   if [ "$1" = "" ]
   then
      return 1
   else
      hostline=$1
   fi
   if [ "$2" = "" ]
   then
      return 1
   else
      description=$2
   fi
   if [ "$3" = "" ]
   then
      return 1
   else
      package=$3
   fi
   if [ "$4" = "" ]
   then
      outFile=${logFile}
   else
      outFile=$4
   fi
   if [ "$5" = "" ]
   then
      failedFile="/dev/null"
   else
      failedFile=$5
   fi
   echo "" >> ${outFile}
   echo "--------------------" >> ${outFile}
   date >> ${outFile}
   echo "" >> ${outFile}

   host=$(echo ${hostline} | cut -f 1 -d ",")
   arch=$(echo ${hostline} | cut -f 2 -d ",")
   dist=$(echo ${hostline} | cut -f 4 -d ",")
   echo "Removing ${description} from host ${host}" >> ${outFile}
   echo "--------------------" >> ${outFile}

   echo $dist | grep deb > /dev/null 2>&1
   if [ $? -eq 0 ]
   then
      cmd="apt-get -y --allow-unauthenticated"
   else
      cmd="yum -y"
      # if we have a zypper repository take zypper
      cmd_check="which zypper"
      eval ssh $sshParameter ${host} \"$cmd_check\" > /dev/null
      if [ $? -eq 0 ]; then cmd="zypper -n"; fi
   fi
   cmd_remote="${cmd} remove ${package}"
   eval ssh $sshParameter ${host} \"$cmd_remote\" >> ${outFile} 2>&1
   if [ $? -ne 0 ]
   then
      error="Failed to remove ${description} on host ${host}"
      echo $error >> ${outFile}
      echo ${host} >> ${failedFile}
      return 1
   fi
   return 0
}

uninstall_meta()
{
   if [ "$1" = "" ]
   then
      return 1
   else
      host=$1
   fi
   if [ "$2" = "" ]
   then
      outFile=${logFile}
   else
      outFile=$2
   fi
   if [ "$3" = "" ]
   then
      failedFile="/dev/null"
   else
      failedFile=$3
   fi

   hostline=$(grep "$1" $metaFile)
   uninstall_package "$hostline" "BeeGFS Meta Server" beegfs-meta $outFile $failedFile
   if [ $? -eq 0 ]
   then
     return 0
   else
     return 1
   fi
}

uninstall_storage()
{
   if [ "$1" = "" ]
   then
      return 1
   else
      host=$1
   fi
   if [ "$2" = "" ]
   then
      outFile=${logFile}
   else
      outFile=$2
   fi
   if [ "$3" = "" ]
   then
      failedFile="/dev/null"
   else
      failedFile=$3
   fi

   hostline=$(grep "$1" $storageFile)
   uninstall_package "$hostline" "BeeGFS Storage Server" beegfs-storage $outFile $failedFile
   if [ $? -eq 0 ]
   then
     return 0
   else
     return 1
   fi
}

uninstall_mgmtd()
{
   if [ "$1" = "" ]
   then
      return 1
   else
      host=$1
   fi
   if [ "$2" = "" ]
   then
      outFile=${logFile}
   else
      outFile=$2
   fi
   if [ "$3" = "" ]
   then
      failedFile="/dev/null"
   else
      failedFile=$3
   fi

   hostline=$(grep "$1" $mgmtdFile)
   uninstall_package "$hostline" "BeeGFS Management Daemon" beegfs-mgmtd $outFile $failedFile
   if [ $? -eq 0 ]
   then
     return 0
   else
     return 1
   fi
}

uninstall_client()
{
   if [ "$1" = "" ]
   then
      return 1
   else
      host=$1
   fi
   if [ "$2" = "" ]
   then
      outFile=${logFile}
   else
      outFile=$2
   fi
   if [ "$3" = "" ]
   then
      failedFile="/dev/null"
   else
      failedFile=$3
   fi

   hostline=$(grep "$1" $clientFile)
   uninstall_package "$hostline" "BeeGFS Helper Daemon" beegfs-helperd $outFile $failedFile
   if [ $? -ne 0 ]
   then
     return 1
   fi

   uninstall_package "$hostline" "BeeGFS Client module" beegfs-client $outFile $failedFile

   if [ $? -eq 0 ]
   then
     return 0
   else
     return 1
   fi
}

uninstall_common()
{
   if [ "$1" = "" ]
   then
      return 1
   else
      host=$1
   fi
   if [ "$2" = "" ]
   then
      outFile=${logFile}
   else
      outFile=$2
   fi
   if [ "$3" = "" ]
   then
      failedFile="/dev/null"
   else
      failedFile=$3
   fi

   hostline=$(cat ${metaFile} ${storageFile} ${clientFile} ${managementFile} | sort | uniq | grep "$1")
   uninstall_package "$hostline" "BeeGFS Common" beegfs-common $outFile $failedFile
   if [ $? -eq 0 ]
   then
     return 0
   else
     return 1
   fi
}


if [ "$1" = "" ];then exit 1; fi
if [ "$2" = "" ];then exit 1; fi

if [ "$1" = "opentk" ]
then
   uninstall_common $2 $3 $4
   exit $?
elif [ "$1" = "meta" ]
then
   uninstall_meta $2 $3 $4
   exit $?
elif [ "$1" = "storage" ]
then
   uninstall_storage $2 $3 $4
   exit $?
elif [ "$1" = "client" ]
then
   uninstall_client $2 $3 $4
   exit $?
elif [ "$1" = "mgmtd" ]
then
   uninstall_mgmtd $2 $3 $4
   exit $?
else
   exit 1
fi
