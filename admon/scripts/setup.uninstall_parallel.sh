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

function uninstall_opentk()
{
IFS="
"
   outFiles=""
   failedFile=$1
   rm ${failedFile} > /dev/null 2>&1
   for line in $(cat ${metaFile} ${storageFile} ${clientFile} ${managementFile} | sort | uniq)
   do
      host=$(echo ${line} | cut -f 1 -d "," | tr -d " ")
      if [ "${host}" = "" ]; then continue; fi

      outFile="${tmpDir}/outFile.${host}"
      outFiles="${outFiles} ${outFile}"
      (bash ${dirname}/setup.uninstall.sh opentk ${host} ${outFile} ${failedFile}) &
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
}

function uninstall_meta()
{
IFS="
"
   outFiles=""
   failedFile=$1
   rm ${failedFile} > /dev/null 2>&1
   for line in $(cat ${metaFile} | sort | uniq)
   do
      echo "inside for"
      host=$(echo ${line} | cut -f 1 -d ",")
      if [ "${host}" = "" ]; then continue; fi
      outFile="${tmpDir}/outFile.${host}"
      outFiles="${outFiles} ${outFile}"
      (bash ${dirname}/setup.uninstall.sh meta ${host} ${outFile} ${failedFile}) &
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
}

function uninstall_storage()
{
IFS="
"
   outFiles=""
   failedFile=$1
   rm ${failedFile} > /dev/null 2>&1
   for line in $(cat ${storageFile} | sort | uniq)
   do
      host=$(echo ${line} | cut -f 1 -d ",")
      if [ "${host}" = "" ]; then continue; fi
      outFile="${tmpDir}/outFile.${host}"
      outFiles="${outFiles} ${outFile}"
      (bash ${dirname}/setup.uninstall.sh storage ${host} ${outFile} ${failedFile}) &
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
}

function uninstall_client()
{
IFS="
"
   outFiles=""
   failedFile=$1
   rm ${failedFile} > /dev/null 2>&1
   for line in $(cat ${clientFile} | sort | uniq)
   do
      host=$(echo ${line} | cut -f 1 -d ",")
      if [ "${host}" = "" ]; then continue; fi
      outFile="${tmpDir}/outFile.${host}"
      outFiles="${outFiles} ${outFile}"
      (bash ${dirname}/setup.uninstall.sh client ${host} ${outFile} ${failedFile}) &
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
}

function uninstall_mgmtd()
{
IFS="
"
   outFiles=""
   failedFile=$1
   rm ${failedFile} > /dev/null 2>&1

   for line in $(cat ${managementFile} | sort | uniq)
   do
      host=$(echo ${line} | cut -f 1 -d ",")
      if [ "${host}" = "" ]; then continue; fi
      outFile="${tmpDir}/outFile.${host}"
      outFiles="${outFiles} ${outFile}"
      (bash ${dirname}/setup.uninstall.sh mgmtd ${host} ${outFile} ${failedFile}) &
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
}

if [ "$1" = "" ];then exit 1; fi

if [ "$1" = "opentk" ]
then
   uninstall_opentk $2
   exit $?
elif [ "$1" = "meta" ]
then
   uninstall_meta $2
   exit $?
elif [ "$1" = "storage" ]
then
   uninstall_storage $2
   exit $?
elif [ "$1" = "client" ]
then
   uninstall_client $2
   exit $?
elif [ "$1" = "mgmtd" ]
then
   uninstall_mgmtd $2
   exit $?
else
   exit 1
fi
