#!/bin/bash

dirname=$(dirname $0)

. ${dirname}/setup.defaults


function check_distri_arch_one_shot()
{
 IFS="
"

 host=$1
 if [ "$host" = "" ]; then return 1; fi

   # check distri
 distri_tag="suse10" # assume suse10 as long as nothing else is proven

   # SuSE check
 cmd="[ -e /etc/SuSE-release ]"
 eval ssh $sshParameter ${host} \"$cmd\"
 if [ $? -eq 0 ]
  then
   cmd="cat /etc/SuSE-release | head -n 1"
   distri=$(eval ssh $sshParameter ${host} \"$cmd\")
   cmd='cat /etc/SuSE-release | grep VERSION | cut -f 2 -d "=" | tr -d " " | cut -f 1 -d "."'
   distri_version=$(eval ssh $sshParameter ${host} $cmd);
   if [ "${distri_version}" = "11" ]
   then
     distri_tag="suse11";
   elif [ "${distri_version}" = "12" ]
   then
     distri_tag="suse12";
   else
     distri_tag="suse12";
   fi
  fi

   # Fedora check
 cmd="[ -e /etc/fedora-release ]"
 eval ssh $sshParameter ${host} \"$cmd\"

 if [ $? -eq 0 ]
 then
   distri_tag="rhel5"
   cmd="cat /etc/fedora-release | head -n 1"
   distri=$(eval ssh $sshParameter ${host} \"$cmd\")
 fi

  # RHEL / SL / Centos
 cmd="[ -e /etc/redhat-release ]"
 eval ssh $sshParameter ${host} \"$cmd\"
 if [ $? -eq 0 ]
 then
   cmd="cat /etc/redhat-release | head -n 1"
   distri=$(eval ssh $sshParameter ${host} \"$cmd\")
   cmd='cat /etc/redhat-release | grep -o [5-7]\.[0-9] | cut -f 1 -d "."'
   distri_version=$(eval ssh $sshParameter ${host} \"$cmd\")
   if [ "${distri_version}" = "5" ]
   then
     distri_tag="rhel5"
   elif [ "${distri_version}" = "6" ]
   then
     distri_tag="rhel6"
   elif [ "${distri_version}" = "7" ]
   then
     distri_tag="rhel7"
   else
     distri_tag="rhel7"
   fi
 fi

  # Debian
 cmd="[ -e /etc/debian_version ]"
 eval ssh $sshParameter ${host} \"$cmd\"
 if [ $? -eq 0 ]
 then
   cmd="cat /etc/debian_version | head -n 1"
   distri="Debian $(eval ssh $sshParameter ${host} \"$cmd\")"
   cmd="cat /etc/debian_version"
   if [ "$(eval ssh $sshParameter ${host} \"$cmd\" | grep 7\.[0-9] | wc -l)" != "0" ]
   then
     distri_tag="deb7"
   elif [ "$(eval ssh $sshParameter ${host} \"$cmd\" | grep 8\.[0-9] | wc -l)" != "0" ]
   then
     distri_tag="deb8"
   elif [ "$(eval ssh $sshParameter ${host} \"$cmd\" | grep jessie | wc -l)" != "0" ]
   then
     distri_tag="deb8"
   elif [ "$(eval ssh $sshParameter ${host} \"$cmd\" | grep wheezy | wc -l)" != "0" ]
   then
     distri_tag="deb7"
   else
     distri_tag="deb8"
   fi
 fi

if [ "$distri" = "" ];
then
   distri="unknown"
   distri_tag="unknown"
   arch="unknown"
else
   # check arch
   arch="i586" # assume x86 32 bit as long as nothing else is proven
   cmd="uname -a | grep 'x86_64'"
   eval ssh $sshParameter ${host} \"$cmd\" > /dev/null
   if [ $? -eq 0 ]; then arch="x86_64"; fi
   cmd="uname -a | grep 'ppc'"
   eval ssh $sshParameter ${host} \"$cmd\" > /dev/null
   if [ $? -eq 0 ]; then arch="ppc"; fi
   cmd="uname -a | grep 'ppc64'"
   eval ssh $sshParameter ${host} \"$cmd\" > /dev/null
   if [ $? -eq 0 ]; then arch="ppc64"; fi
fi

 # note (infodirLockfile): "sed" renames the modified file, so we need a separate file as lockfile

 # strip old (possibly outdated) information
 flock ${infodirLockfile} sed -i 's!'"${host}",.*'!'"${host}"'!' ${metaFile}
 flock ${infodirLockfile} sed -i 's!'"${host}",.*'!'"${host}"'!' ${storageFile}
 flock ${infodirLockfile} sed -i 's!'"${host}",.*'!'"${host}"'!' ${clientFile}
 flock ${infodirLockfile} sed -i 's!'"${host}",.*'!'"${host}"'!' ${managementFile}

 # write the new config
 flock ${infodirLockfile} sed -i 's!'"${host}"\$'!'"${host}"','"${arch}"','"${distri}"','"${distri_tag}"'!' ${metaFile}
 flock ${infodirLockfile} sed -i 's!'"${host}"\$'!'"${host}"','"${arch}"','"${distri}"','"${distri_tag}"'!' ${storageFile}
 flock ${infodirLockfile} sed -i 's!'"${host}"\$'!'"${host}"','"${arch}"','"${distri}"','"${distri_tag}"'!' ${clientFile}
 flock ${infodirLockfile} sed -i 's!'"${host}"\$'!'"${host}"','"${arch}"','"${distri}"','"${distri_tag}"'!' ${managementFile}

 return 0
}


if [ "$1" = "" ]
then
 exit 1
fi

check_distri_arch_one_shot $1
if [ $? -ne 0 ]; then exit 1; fi
