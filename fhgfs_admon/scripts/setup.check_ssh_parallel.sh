#!/bin/bash

dirname=$(dirname $0)

. ${dirname}/setup.defaults


IFS="
"

NUM=0
QUEUE=""
MAX_NPROC=10
COUNTER=0

function queue() {
   QUEUE="$QUEUE $1"
   NUM=$(($NUM+1))
}

function regeneratequeue() {
   OLDREQUEUE=$QUEUE
   QUEUE=""
   NUM=0
   for PID in $OLDREQUEUE
   do
      if [ -d /proc/$PID  ] ; then
         QUEUE="$QUEUE $PID"
         NUM=$(($NUM+1))
      fi
   done
}

function checkqueue() {
   OLDCHQUEUE=$QUEUE
   for PID in $OLDCHQUEUE
   do
      if [ ! -d /proc/$PID ] ; then
         regeneratequeue # at least one PID has finished
         break
      fi
   done
}

function check_ssh()
{
   outFile=$sshFailedLogFile
   rm -f $outFile > /dev/null 2>&1

IFS="
"
   for host in $(echo "$1" | tr "," "\n" | sort | uniq)
   do
      if [ "${host}" = "" ]; then continue; fi
      (bash ${dirname}/setup.check_ssh.sh ${host} ${outFile}) &
      PID=$!
      queue $PID

      while [ $NUM -ge $MAX_NPROC ]; do
         checkqueue
         sleep 0.4
      done

   done
   wait

   if [ -e $outFile ]
   then
     cat $outFile
   else
     touch $outFile
   fi
}

if [ "$1" = "" ]
then
   exit 1
fi

hosts=$1

check_ssh $hosts

exit $?
