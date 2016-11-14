#!/bin/bash

dirname=$(dirname $0)

. ${dirname}/setup.defaults


if [ "$1" = "" ];then exit 1;fi
if [ "$2" = "" ];then exit 1;fi

host=$1
filepath=$2
lines=$3

if [ "${lines}" = "" ]; then lines=0; fi

if [ ${lines} -eq 0 ]
then
  cmd="cat ${filepath}"
  eval ssh $sshParameter ${host} \"$cmd\"
else
  cmd="tail -n ${lines} ${filepath}"
  eval ssh $sshParameter ${host} \"$cmd\"
fi 
