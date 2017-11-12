#!/bin/bash

dirname=$(dirname $0)

. ${dirname}/setup.defaults


IFS="
"

NUM=0
QUEUE=""
MAX_NPROC=4
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




function stop_meta()
{
IFS="
"
   failed=""
   REMOTECMD="/etc/init.d/beegfs-meta stop"
   for line in $(cat ${metaFile})
   do
      host=$(echo ${line} | cut -f 1 -d ",")
      (eval ssh $sshParameter ${host} \"${REMOTECMD}\" > ${tmpStartStopDir}/beegfs_setup.meta.${host}.tmp 2>&1; if [ $? -eq 0 ]; then rm ${tmpStartStopDir}/beegfs_setup.meta.${host}.tmp; fi ) &
      PID=$!
      queue $PID

      while [ $NUM -ge $MAX_NPROC ]; do
         checkqueue
         sleep 0.1
      done

   done
   wait # wait for ssh $sshParameter jobs to finish before continuing
   for f in $(find ${tmpStartStopDir}/ -name beegfs_setup.meta.*.tmp)
   do
      h=$(echo ${f} | cut -d "." -f 3)
      failed="${failed} ${h}"
      for line in $(cat ${f}); do echo "[${h},stop beegfs-meta] ${line}" >> ${logFile}; done
   done
   IFS=" "
   if [ "${failed}" != "" ]
   then
      for host in ${failed}; do echo ${host}; done
      rm ${tmpStartStopDir}/beegfs_setup.meta.*.tmp > /dev/null 2>&1
      return 1
   fi

}

function stop_storage()
{
IFS="
"
   failed=""
   REMOTECMD="/etc/init.d/beegfs-storage stop"
   for line in $(cat ${storageFile})
   do
      host=$(echo ${line} | cut -f 1 -d ",")
      (eval ssh $sshParameter ${host} \"${REMOTECMD}\" > ${tmpStartStopDir}/beegfs_setup.storage.${host}.tmp 2>&1; if [ $? -eq 0 ]; then rm ${tmpStartStopDir}/beegfs_setup.storage.${host}.tmp; fi ) &
      PID=$!
      queue $PID

      while [ $NUM -ge $MAX_NPROC ]; do
         checkqueue
         sleep 0.1
      done

   done
   wait # wait for ssh $sshParameter jobs to finish before continuing
   for f in $(find ${tmpStartStopDir}/ -name beegfs_setup.storage.*.tmp)
   do
      h=$(echo ${f} | cut -d "." -f 3)
      failed="${failed} ${h}"
      for line in $(cat ${f}); do echo "[${h},stop beegfs-storage] ${line}" >> ${logFile}; done
   done
   IFS=" "
   if [ "${failed}" != "" ]
   then
      for host in ${failed}; do echo ${host}; done
      rm ${tmpStartStopDir}/beegfs_setup.storage.*.tmp > /dev/null 2>&1
      return 1
   fi

}

function stop_mgmtd()
{
IFS="
"
   failed=""
   REMOTECMD="/etc/init.d/beegfs-mgmtd stop"
   for line in $(cat ${managementFile})
   do
      host=$(echo ${line} | cut -f 1 -d ",")
      (eval ssh $sshParameter ${host} \"${REMOTECMD}\" > ${tmpStartStopDir}/beegfs_setup.mgmtd.${host}.tmp 2>&1; if [ $? -eq 0 ]; then rm ${tmpStartStopDir}/beegfs_setup.mgmtd.${host}.tmp; fi ) &
      PID=$!
      queue $PID

      while [ $NUM -ge $MAX_NPROC ]; do
         checkqueue
         sleep 0.1
      done

   done
   wait # wait for ssh $sshParameter jobs to finish before continuing
   for f in $(find ${tmpStartStopDir}/ -name beegfs_setup.mgmtd.*.tmp)
   do
      h=$(echo ${f} | cut -d "." -f 3)
      failed="${failed} ${h}"
      for line in $(cat ${f}); do echo "[${h},stop beegfs-mgmtd] ${line}" >> ${logFile}; done
   done
   IFS=" "
   if [ "${failed}" != "" ]
   then
      for host in ${failed}; do echo ${host}; done
      rm ${tmpStartStopDir}/beegfs_setup.mgmtd.*.tmp > /dev/null 2>&1
      return 1
   fi

}

function stop_admon()
{
IFS="
"
   failed=""
   line=$(cat ${admonFile})
   if [ "${line}" != "" ]
   then
      host=$(echo ${line} | cut -f 1 -d ",")
      cmd="/etc/init.d/beegfs-admon stop"
      eval ssh $sshParameter ${host} \"$cmd\" | tee -a ${tmpFile}
      if [ $? -ne 0 ]
      then
         failed="${failed}${host} "
      fi
   fi
   if [ "${failed}" != "" ]
   then
      for line in $(cat $tmpFile); do echo "[${host},stop beegfs-admon] ${line}" >> ${logFile}; done
     cat > $tmpFile <<EOF
Failed to start the BeeGFS Monitoring System on the following hosts :

`for host in ${failed}; do echo ${host}; done`

Detailed information can be found in `echo ${logFile}`
EOF
      dia_textbox "Error Summary" $tmpFile
      rm $tmpFile > /dev/null 2>&1
   fi

}

function stop_client()
{
   #  failed_helperd=""
   #   failed_client=""
IFS="
"
   for line in $(cat ${clientFile})
   do
      REMOTECMD="/etc/init.d/beegfs-helperd stop"
      host=$(echo ${line} | cut -f 1 -d ",")
      (eval ssh $sshParameter ${host} \"${REMOTECMD}\" > ${tmpStartStopDir}/beegfs_setup.helperd.${host}.tmp 2>&1; if [ $? -eq 0 ]; then rm ${tmpStartStopDir}/beegfs_setup.helperd.${host}.tmp; fi ) &
      PID=$!
      queue $PID

      while [ $NUM -ge $MAX_NPROC ]
      do
         checkqueue
         sleep 0.1
      done

      REMOTECMD="/etc/init.d/beegfs-client stop"
      (eval ssh $sshParameter ${host} \"${REMOTECMD}\" > ${tmpStartStopDir}/beegfs_setup.client.${host}.tmp 2>&1; if [ $? -eq 0 ]; then rm ${tmpStartStopDir}/beegfs_setup.client.${host}.tmp; fi ) &
      PID=$!
      queue $PID

      while [ $NUM -ge $MAX_NPROC ]; do
         checkqueue
         sleep 0.1
      done
   done
   wait # wait for ssh $sshParameter jobs to finish before continuing
   for f in $(find ${tmpStartStopDir}/ -name beegfs_setup.helperd.*.tmp)
   do
      h=$(echo ${f} | cut -d "." -f 3)
      failed_helperd="${failed_helperd} ${h}"
      for line in $(cat ${f}); do echo "[${h},stop beegfs-helperd] ${line}" >> ${logFile}; done
   done

   for f in $(find ${tmpStartStopDir}/ -name beegfs_setup.client.*.tmp)
   do
      h=$(echo ${f} | cut -d "." -f 3)
      failed_client="${failed_client} ${h}"
      for line in $(cat ${f}); do echo "[${h},stop beegfs-client] ${line}" >> ${logFile}; done
   done


   failed=0
   IFS=" "
   if [ "${failed_helperd}" != "" ]
   then

      for host in ${failed_helperd}; do echo ${host}; done

      rm ${tmpStartStopDir}/beegfs_setup.helperd.*.tmp > /dev/null 2>&1
      failed=1
   fi

   if [ "${failed_client}" != "" ]
   then

      for host in ${failed_client}; do echo ${host}; done

      rm ${tmpStartStopDir}/beegfs_setup.client.*.tmp > /dev/null 2>&1
      failed=1
   fi
   if [ "$failed" == "1" ]
   then
      return 1
   fi
}

if [ "$1" = "" ];
then
   exit 1
fi

if [ "$1" = "meta" ]
then
   stop_meta
   exit $?
elif [ "$1" = "storage" ]
then
   stop_storage
   exit $?
elif [ "$1" = "mgmtd" ]
then
   stop_mgmtd
   exit $?
elif [ "$1" = "client" ]
then
   stop_client
   exit $?
else
   exit 1
fi
