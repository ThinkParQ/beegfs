#!/bin/bash

dirname=$(dirname $0)

. ${dirname}/setup.defaults


function start_meta()
{
   if [ "$1" = "" ]; then return 1; else host=$1;fi
   REMOTECMD="/etc/init.d/beegfs-meta start"
   eval ssh $sshParameter ${host} \"${REMOTECMD}\"
   if [ $? -eq 0 ]; then return 0; else return 1; fi
}

function start_storage()
{
   if [ "$1" = "" ]; then return 1; else host=$1;fi
   REMOTECMD="/etc/init.d/beegfs-storage start"
   eval ssh $sshParameter ${host} \"${REMOTECMD}\"
   if [ $? -eq 0 ]; then return 0; else return 1; fi
}

function start_client()
{
   if [ "$1" = "" ]; then return 1; else host=$1;fi
   REMOTECMD="/etc/init.d/beegfs-helperd start"
   eval ssh $sshParameter ${host} \"${REMOTECMD}\"
   if [ $? -ne 0 ]; then return 1; fi
   REMOTECMD="/etc/init.d/beegfs-client start"
   eval ssh $sshParameter ${host} \"${REMOTECMD}\"
   if [ $? -eq 0 ]; then return 0; else return 1; fi
}

function start_mgmtd()
{
   if [ "$1" = "" ]; then return 1; else host=$1;fi
   REMOTECMD="/etc/init.d/beegfs-mgmtd start"
   eval ssh $sshParameter ${host} \"${REMOTECMD}\"
   if [ $? -eq 0 ]; then return 0; else return 1; fi
}


if [ "$1" = "" ];then exit 1; fi
if [ "$2" = "" ];then exit 1; fi


if [ "$1" = "meta" ]
then
   start_meta $2
   exit $?
elif [ "$1" = "storage" ]
then
   start_storage $2
   exit $?
elif [ "$1" = "client" ]
then
   start_client $2
   exit $?
elif [ "$1" = "mgmtd" ]
then
   start_mgmtd $2
   exit $?
else
   exit 1
fi
