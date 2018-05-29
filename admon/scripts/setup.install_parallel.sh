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

function deploy_meta()
{
IFS="
"
   outFiles=""
   failedFile=$1
   rm ${failedFile} > /dev/null 2>&1

   for line in $(cat ${metaFile} | sort | uniq)
   do
      host=$(echo ${line} | cut -f 1 -d ",")
      if [ "${host}" = "" ]; then continue; fi
      outFile="${tmpDir}/outFile.${host}"
      outFiles="${outFiles} ${outFile}"
      (bash ${dirname}/setup.install.sh meta ${host} ${outFile} ${failedFile}) &
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

function deploy_storage()
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
      (bash ${dirname}/setup.install.sh storage ${host} ${outFile} ${failedFile}) &
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

function deploy_client()
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
      (bash ${dirname}/setup.install.sh client ${host} ${outFile} ${failedFile}) &
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

function deploy_mgmtd()
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
      (bash ${dirname}/setup.install.sh mgmtd ${host} ${outFile} ${failedFile}) &
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

#common and opentk package will be installed with the package dependencies and not with the script
if [ "$1" = "meta" ]
then
   deploy_meta $2
   exit $?
elif [ "$1" = "storage" ]
then
   deploy_storage $2
   exit $?
elif [ "$1" = "client" ]
then
   if [ -f "${tmpClientWarning}" ]
   then
      rm ${tmpClientWarning}
   fi
   touch ${tmpClientWarning}
   deploy_client $2
   exit $?
elif [ "$1" = "mgmtd" ]
then
   deploy_mgmtd $2
   exit $?
else
   echo "FFF"
   exit 1
fi
