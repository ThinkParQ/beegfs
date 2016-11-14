#!/bin/bash

dirname=$(dirname $0)

. ${dirname}/setup.defaults


function write_config() {
   host=$1
   outFile=$2
   clientMount=$(grep clientMount ${confdir}/config | cut -d "=" -f 2)
   cmd="echo \"${clientMount} /etc/beegfs/beegfs-client.conf\" > /etc/beegfs/beegfs-mounts.conf"
   eval ssh $sshParameter ${host} \"$cmd\" >> ${outFile} 2>&1

   return $?
}

if [ "$1" = "" ];then exit 1;fi
if [ "$2" = "" ];then exit 1;fi

write_config $1 $2
if [ $? -ne 0 ];then exit 1;fi
