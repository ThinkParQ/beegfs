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




function check_distri_arch()
{
  hosts=$(cat ${metaFile} ${storageFile} ${clientFile} ${managementFile} | cut -f 1 -d "," | sort -u | tr -d " ")

  for host in $hosts
  do
    (bash ${dirname}/setup.get_system_info.sh ${host}) &
    PID=$!
    queue PID
    while [ $NUM -ge $MAX_NPROC ]; do
            checkqueue
            sleep 0.1
    done
  done
  wait
}

check_distri_arch
