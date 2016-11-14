#!/bin/bash

dirname=$(dirname $0)

. ${dirname}/setup.defaults


if [ "$1" = "" ]
then
  exit 1
fi

host=$1

if [ "$2" = "" ]
then
  exit 1
fi

outFile=$2

cmd="echo A > /dev/null"
eval ssh $sshParameter $host \"$cmd\"

if [ $? -ne 0 ]
then
  #     status="Not Running"
  flock ${outFile} /bin/bash -c "echo ${host} >> ${outFile}" 2> /dev/null
  exit 1
fi

exit 0
