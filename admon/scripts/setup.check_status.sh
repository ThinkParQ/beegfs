#!/bin/bash

dirname=$(dirname $0)

. ${dirname}/setup.defaults


function check_status()
{
   deamon=$1
   host=$2
   outfile=$3

   if [ "$host" != "" ]
   then
      cmd="/etc/init.d/beegfs-$deamon status"
      eval ssh $sshParameter $host \"$cmd\" > /dev/null

      if [ $? -eq 0 ]
      then
        #     status="Running"
        status="true"
      else
        #     status="Not Running"
        status="false"
      fi

      info="${host},${status}";
      flock ${outFile} /bin/bash -c "echo $info >> ${outFile}" 2> /dev/null

   else
      return 1
   fi
}

if [ "$1" = "" ]
then
   exit 1
fi

deamon=$1

if [ "$2" = "" ]
then
   exit 1
fi

host=$2

if [ "$3" = "" ]
then
   outFile=/dev/stdout
else
   outFile=$3
fi

check_status $deamon $host $outfile
exit $?
