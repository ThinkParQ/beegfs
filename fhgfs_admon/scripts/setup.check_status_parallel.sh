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

function check_meta()
{
   outFile=${tmpSetupCheck}
   rm -f $outFile > /dev/null 2>&1
IFS="
"
   for line in $(cat ${metaFile} | sort | uniq)
   do
      host=$(echo ${line} | cut -f 1 -d ",")
      if [ "${host}" = "" ]; then continue; fi
      (bash ${dirname}/setup.check_status.sh meta ${host} ${outFile}) &
      PID=$!
      queue $PID
      
      while [ $NUM -ge $MAX_NPROC ]; do
         checkqueue
         sleep 0.1
      done
      
   done
   wait
   cat $outFile
}

function check_storage()
{
   outFile=${tmpSetupCheck}
   rm -f $outFile > /dev/null 2>&1
IFS="
"
   for line in $(cat ${storageFile} | sort | uniq)
   do
      host=$(echo ${line} | cut -f 1 -d ",")
      if [ "${host}" = "" ]; then continue; fi
      (bash ${dirname}/setup.check_status.sh storage ${host} ${outFile}) &
      PID=$!
      queue $PID
      
      while [ $NUM -ge $MAX_NPROC ]; do
         checkqueue
         sleep 0.1
      done
      
   done
   wait
   cat $outFile
}

function check_mgmtd()
{
   outFile=${tmpSetupCheck}
   rm -f $outFile > /dev/null 2>&1
IFS="
"
   for line in $(cat ${managementFile} | sort | uniq)
   do
      host=$(echo ${line} | cut -f 1 -d ",")
      if [ "${host}" = "" ]; then continue; fi
      (bash ${dirname}/setup.check_status.sh mgmtd ${host} ${outFile}) &
      PID=$!
      queue $PID
      
      while [ $NUM -ge $MAX_NPROC ]; do
         checkqueue
         sleep 0.1
      done
      
   done
   wait
   cat $outFile
}

function check_client()
{
   outFile=${tmpSetupCheck}
   rm -f $outFile > /dev/null 2>&1
IFS="
"
   for line in $(cat ${clientFile} | sort | uniq)
   do
      host=$(echo ${line} | cut -f 1 -d ",")
      if [ "${host}" = "" ]; then continue; fi
      (bash ${dirname}/setup.check_status.sh client ${host} ${outFile}) &
      PID=$!
      queue $PID
      
      while [ $NUM -ge $MAX_NPROC ]; do
         checkqueue
         sleep 0.1
      done
      
   done
   wait
   cat $outFile
}



if [ "$1" = "" ];then exit 1; fi

if [ "$1" = "meta" ]
then
   check_meta $2
   exit $?
elif [ "$1" = "storage" ]
then
   check_storage $2
   exit $?
elif [ "$1" = "client" ]
then
   check_client $2
   exit $?
elif [ "$1" = "mgmtd" ]
then
   check_mgmtd $2
   exit $?
else
   exit 1
fi
