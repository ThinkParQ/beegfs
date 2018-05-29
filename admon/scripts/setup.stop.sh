#!/bin/bash

dirname=$(dirname $0)

. ${dirname}/setup.defaults


function stop_meta()
{
   if [ "$1" = "" ]; then return 1; else host=$1;fi
   REMOTECMD="/etc/init.d/beegfs-meta stop"
   eval ssh $sshParameter ${host} \"${REMOTECMD}\"
   if [ $? -eq 0 ]; then return 0; else return 1; fi
}

function stop_storage()
{
   if [ "$1" = "" ]; then return 1; else host=$1;fi
   REMOTECMD="/etc/init.d/beegfs-storage stop"
   eval ssh $sshParameter ${host} \"${REMOTECMD}\"
   if [ $? -eq 0 ]; then return 0; else return 1; fi
}

function stop_mgmtd()
{
   if [ "$1" = "" ]; then return 1; else host=$1;fi
   REMOTECMD="/etc/init.d/beegfs-mgmtd stop"
   eval ssh $sshParameter ${host} \"${REMOTECMD}\"
   if [ $? -eq 0 ]; then return 0; else return 1; fi
}

function stop_client()
{
   if [ "$1" = "" ]; then return 1; else host=$1;fi
   REMOTECMD="/etc/init.d/beegfs-helperd stop"
   eval ssh $sshParameter ${host} \"${REMOTECMD}\"
   if [ $? -ne 0 ]; then return 1; fi
   REMOTECMD="/etc/init.d/beegfs-client stop"
   eval ssh $sshParameter ${host} \"${REMOTECMD}\"
   if [ $? -eq 0 ]; then return 0; else return 1; fi
}

if [ "$1" = "" ];then exit 1; fi
if [ "$2" = "" ];then exit 1; fi


if [ "$1" = "meta" ]
then
   stop_meta $2
   exit $?
elif [ "$1" = "storage" ]
then
   stop_storage $2
   exit $?
elif [ "$1" = "mgmtd" ]
then
   stop_mgmtd $2
   exit $?
elif [ "$1" = "client" ]
then
   stop_client $2
   exit $?
else
   exit 1
fi
