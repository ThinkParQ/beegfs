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

   if [ "$1" = "" ]; then exit 1; fi
   outFiles=""
   failedFile=$1
   rm ${failedFile} > /dev/null 2>&1

   for line in $(cat ${metaFile} ${storageFile} ${clientFile} ${managementFile} | sort | uniq)
   do
      host=$(echo ${line} | cut -f 1 -d "," | tr -d " ")
      if [ "${host}" = "" ]; then continue; fi
      distri=$(echo ${line} | cut -f 4 -d "," | tr -d " ")
      if [ "${distri}" = "" ]; then continue; fi

      outFile="${tmpDir}/outFileRepo.${host}"
      outFiles="${outFiles} ${outFile}"

      (bash ${dirname}/setup.add_repo.sh ${host} ${distri} ${failedFile} ${outFile}) &
      PID=$!
      queue $PID

      while [ $NUM -ge $MAX_NPROC ]; do
         checkqueue
         sleep 0.1
      done

   done
   wait


   IFS=" "
   for file in ${outFiles}
   do
      cat ${file} 2>/dev/null | tee -a ${logFile}
      rm ${file} > /dev/null 2>&1
   done

   exit 0
