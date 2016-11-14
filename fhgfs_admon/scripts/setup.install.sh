#!/bin/bash

dirname=$(dirname $0)

. ${dirname}/setup.defaults


check_suse_client_dependencies()
{
   if [ "$1" = "" ]
   then
      return 1
   fi

   host=$1

   if [ "$2" = "" ]
   then
      outFile=${logFile}
   else   
      outFile=$2
   fi
 
   #SuSe with package manager zypper
   cmd="which zypper"
   eval ssh $sshParameter ${host} \"$cmd\" > /dev/null
   if [ $? -eq 0 ]
   then
      cmd="zypper search -i kernel-source | grep kernel-source"
      eval ssh $sshParameter ${host} \"${cmd}\" > /dev/null 2> ${outFile}

      if [ $? -ne 0 ]
      then
         flock ${tmpClientWarning} /bin/bash -c "echo ${host} >> ${tmpClientWarning}" 2> ${outFile}
         echo "WARNING: ${host} - Please check if the kernel module development packages (kernel-source) is installed for the current kernel version." >> ${outFile} 2>&1
         return 1
      fi

   #SuSe with package manager yum
   else
      cmd="yum list installed kernel-source | grep kernel-source"
      eval ssh $sshParameter ${host} \"${cmd}\" > /dev/null 2> ${outFile}     

      if [ $? -ne 0 ]
      then
         flock ${tmpClientWarning} /bin/bash -c "echo ${host} >> ${tmpClientWarning}" 2> ${outFile}
         echo "WARNING: ${host} - Please check if the kernel module development packages (kernel-source) is installed for the current kernel version." >> ${outFile} 2>&1
	 return 1
      fi
   fi

   return 0
}

check_redhat_client_dependencies()
{
   if [ "$1" = "" ]
   then
      return 1
   fi

   host=$1

   if [ "$2" = "" ]
   then
      outFile=${logFile}
   else   
      outFile=$2
   fi

   #RedHat with package manager yum
   cmd="yum list installed kernel-devel | grep kernel-devel"
   eval ssh $sshParameter ${host} \"${cmd}\" > /dev/null 2> ${outFile}

   if [ $? -ne 0 ]
   then
      flock ${tmpClientWarning} /bin/bash -c "echo ${host} >> ${tmpClientWarning}" 2> ${outFile}
      echo "WARNING: ${host} - Please check if the kernel module development package (kernel-devel) is installed for the current kernel version." >> ${outFile} 2>&1
      return 1
   fi

   return 0
}

check_debian_client_dependencies()
{
   if [ "$1" = "" ]
   then
      return 1
   fi

   host=$1

   if [ "$2" = "" ]
   then
      outFile=${logFile}
   else   
      outFile=$2
   fi

   #Debian with package manager dpkg
   cmd="dpkg -l linux-headers* | grep linux-headers | grep ii |grep meta"
   eval ssh $sshParameter ${host} \"${cmd}\" > /dev/null 2> ${outFile}

   if [ $? -ne 0 ]
   then
      flock ${tmpClientWarning} /bin/bash -c "echo ${host} >> ${tmpClientWarning}" 2> ${outFile}
      echo "WARNING: ${host} - Please check if the kernel module development package (linux-headers) is installed for the current kernel version." >> ${outFile} 2>&1
      return 1
   fi

   return 0
}

install_package()
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
   
   #this is necessary because older yum version install both 
   # 64 bit as well as 32 bit on 64bit machines if you do not force 64bit only
   if [ "${arch}" = "x86_64" ]
   then
      YUM_EXT=".x86_64"
   fi

   echo "Installing ${description} on host ${host}" >> ${outFile}
   echo "--------------------" >> ${outFile}
  
   echo $dist | grep deb > /dev/null 2>&1 
   if [ $? -eq 0 ]
   then
      if [ "${package}" = "beegfs-client" ]
      then
         check_debian_client_dependencies ${host} ${outFile}

         # No architecture extension for the package is required, because the 
         # client is a noarch package
         YUM_EXT=""
      fi
      cmd="apt-get -y --allow-unauthenticated"
      YUM_EXT=""
   else
      echo $dist | grep suse > /dev/null 2>&1 
      if [ $? -eq 0 ]
      then
         if [ "${package}" = "beegfs-client" ]
         then
            check_suse_client_dependencies ${host} ${outFile}

            # No architecture extension for the package is required, because the 
            # client is a noarch package
            YUM_EXT=""
         fi
      else
         if [ "${package}" = "beegfs-client" ]
         then
            check_redhat_client_dependencies ${host} ${outFile}

            # No architecture extension for the package is required, because the 
            # client is a noarch package
            YUM_EXT=""
         fi  
      fi
      cmd="yum -y"
      # if we have a zypper repository take zypper
      cmd_check="which zypper"
      eval ssh $sshParameter ${host} \"$cmd_check\" > /dev/null
      if [ $? -eq 0 ];
         then
            cmd="zypper -n --no-gpg-checks"
            YUM_EXT=""
         fi
   fi

   cmd_remote="${cmd} install ${package}${YUM_EXT}"
   eval ssh $sshParameter ${host} \"$cmd_remote\" >> ${outFile} 2>&1

   if [ $? -ne 0 ]
   then
      error="Failed to install ${description} on host ${host}"
      echo $error >> ${outFile}
      echo ${host} >> ${failedFile}
      return 1
   fi

   return 0
}

deploy_meta()
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
   install_package "$hostline" "BeeGFS Meta Server" beegfs-meta $outFile $failedFile
   if [ $? -eq 0 ]
   then 
     bash ${dirname}/setup.write_config.sh ${host} /etc/beegfs/beegfs-meta.conf ${outFile}
     if [ $? -ne 0 ]
     then
        echo "Can not write configuration file: /etc/beegfs/beegfs-meta.conf on host: ${host}" >> $outFile
        echo ${host} >> ${failedFile}
        return 1
     fi
   else
     return 1
   fi
}

deploy_storage()
{
   if [ "$1" = "" ]
   then
      return 1
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
   install_package "$hostline" "BeeGFS Storage Server" beegfs-storage $outFile $failedFile
   if [ $? -eq 0 ]
   then 
     bash ${dirname}/setup.write_config.sh ${host} /etc/beegfs/beegfs-storage.conf ${outFile}
     if [ $? -ne 0 ]
     then
        echo "Can not write configuration file: /etc/beegfs/beegfs-storage.conf on host: ${host}" >> $outFile
        echo ${host} >> ${failedFile}
        return 1
     fi
   else
     return 1
   fi
}

deploy_mgmtd()
{
   if [ "$1" = "" ]
   then
      return 1
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

   hostline=$(grep "$1" $managementFile)
   install_package "$hostline" "BeeGFS Management Daemon" beegfs-mgmtd $outFile $failedFile
   if [ $? -eq 0 ]
   then 
     bash ${dirname}/setup.write_config.sh ${host} /etc/beegfs/beegfs-mgmtd.conf ${outFile}
     if [ $? -ne 0 ]
     then
        echo "Can not write configuration file: /etc/beegfs/beegfs-mgmtd.conf on host: ${host}" >> $outFile
        echo ${host} >> ${failedFile}
        return 1
     fi
   else
     return 1
   fi
}

deploy_client()
{
   if [ "$1" = "" ]
   then
      return 1
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

   deploy_utils $1 $2 $3
   if [ $? -ne 0 ]; then return 1; fi

   install_package "$hostline" "BeeGFS Helper Daemon" beegfs-helperd $outFile $failedFile  
   if [ $? -eq 0 ]
   then 
     bash ${dirname}/setup.write_config.sh ${host} /etc/beegfs/beegfs-helperd.conf ${outFile}
     if [ $? -ne 0 ]
     then
        echo "Can not write configuration file: /etc/beegfs/beegfs-helperd.conf on host: ${host}" >> $outFile
        echo ${host} >> ${failedFile}
        return 1
     fi
     else
        return 1
   fi
   
   install_package "$hostline" "BeeGFS Client module" beegfs-client $outFile $failedFile
   if [ $? -ne 0 ]; then return 1; fi   

   # check for ib options and compile client
   IB_OPTIONS=$(grep ${host} ${ib_hosts_file} | cut -f 2 -d ",")
   if [ "${IB_OPTIONS}" = "" ]
   then
     # no IB
     COMPILE_OPTIONS="-j8"
   else 
     for PARAM in ${IB_OPTIONS}
     do
       eval $PARAM
     done
     if [ "${OFED_KERNEL_INCLUDE_PATH}" != "" ]
     then
       COMPILE_OPTIONS="-j8 BEEGFS_OPENTK_IBVERBS=1 OFED_INCLUDE_PATH=${OFED_KERNEL_INCLUDE_PATH}"
     else
       COMPILE_OPTIONS="-j8 BEEGFS_OPENTK_IBVERBS=1"
     fi
   fi

   # look if make is installed 
   TOOLS_TO_CHECK="make g++"
   for tool in $TOOLS_TO_CHECK
   do
     cmd="which ${tool}"
     eval ssh $sshParameter ${host} \"$cmd\" > /dev/null 2>&1
     if [ $? -ne 0 ]
     then
       echo "" >> ${outFile}
       error="Failed to compile client module on host ${host}. No '${tool}' found."
       echo $error >> $outFile
       echo ${host} >> ${failedFile}
       return 1
     fi
   done

   cmd="sed -i 's!buildArgs=.*!buildArgs=${COMPILE_OPTIONS}!g' ${AUTOBUILD_CONFIG}"
   eval ssh $sshParameter ${host} \"$cmd\" >> ${outFile} 2>&1
   if [ $? -ne 0 ]
   then
      echo "Can not write configuration file: ${AUTOBUILD_CONFIG} on host: ${host}" >> $outFile
      echo ${host} >> ${failedFile}
      return 1
   fi

   cmd="/etc/init.d/beegfs-client rebuild"
   eval ssh $sshParameter ${host} \"$cmd\" >> ${outFile} 2>&1
   if [ $? -ne 0 ]
   then
      echo "" >> ${outFile}
      error="Failed to compile client module on host ${host}"
      echo $error >> $outFile
      echo "Options have been : ${COMPILE_OPTIONS}" >> $outFile
      echo ${host} >> ${failedFile}
      return 1
   fi

   cmd="rm -f /var/lib/beegfs/client/force-auto-build"
   eval ssh $sshParameter ${host} \"$cmd\" >> ${outFile} 2>&1

   bash ${dirname}/setup.write_config.sh ${host} /etc/beegfs/beegfs-client.conf ${outFile}
   if [ $? -ne 0 ]
   then
      echo "Can not write configuration file: /etc/beegfs/beegfs-client.conf on host: ${host}" >> $outFile
      echo ${host} >> ${failedFile}
      return 1
   fi

   bash ${dirname}/setup.write_mounts.sh ${host} ${outFile}
   if [ $? -ne 0 ]
   then
      echo "Can not write configuration file: /etc/beegfs/beegfs-mounts.conf on host: ${host}" >> $outFile
      echo ${host} >> ${failedFile}
      return 1
   fi
}

deploy_utils()
{
   if [ "$1" = "" ]
   then
      return 1
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
   install_package "$hostline" "BeeGFS Utilities" beegfs-utils $outFile $failedFile
   if [ $? -ne 0 ]
   then
     return 1
   fi
}

deploy_common()
{
   if [ "$1" = "" ]
   then
      return 1
   else
      HOST=$1
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

   hostline=$(cat ${metaFile} ${storageFile} ${clientFile} ${managementFile} | sort | uniq | grep "${HOST}")
   install_package "$hostline" "BeeGFS Common" beegfs-common $outFile $failedFile
   if [ $? -ne 0 ]
   then 
     return 1
   fi
}


if [ "$1" = "" ];then exit 1; fi
if [ "$2" = "" ];then exit 1; fi

#common and opentk package will be installed with the package dependencies and not with the script
if [ "$1" = "meta" ]
then
   deploy_meta $2 $3 $4
   exit $?
elif [ "$1" = "storage" ]
then
   deploy_storage $2 $3 $4
   exit $?
elif [ "$1" = "client" ]
then
   deploy_client $2 $3 $4
   exit $?
elif [ "$1" = "mgmtd" ]
then
   deploy_mgmtd $2 $3 $4
   exit $?
elif [ "$1" = "utils" ]
then
   deploy_utils $2 $3 $4
   exit $?
else
   exit 1
fi
