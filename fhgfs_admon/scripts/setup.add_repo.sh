#!/bin/bash

dirname=$(dirname $0)

. ${dirname}/setup.defaults


BASE_REPO_PATH=${base_repo_url}/dists
RPM_GPG_KEY="RPM-GPG-KEY-beegfs"
RPM_GPG_KEY_URL="${base_repo_url}/gpg/${RPM_GPG_KEY}"
DEB_GPG_KEY="DEB-GPG-KEY-beegfs"
DEB_GPG_KEY_URL="${base_repo_url}/gpg/${DEB_GPG_KEY}"

function add_repo_zypper()
{
  if [ "$1" = "" ]; then return; fi
  if [ "$2" = "" ]; then return; fi
  if [ "$3" = "" ]; then return; fi
  HOST=$1
  REPOFILE=beegfs-$2.repo
  LOGFILE=$3
  REPO=${BASE_REPO_PATH}/${REPOFILE}

  cmd="wget $wgetParameters ${REPO} -O /etc/zypp/repos.d/${REPOFILE}"
  eval ssh $sshParameter $HOST \"$cmd\" > /dev/null 2>> ${LOGFILE}
  if [ $? -ne 0 ]
  then
    echo "Can not download the repository configuration file: ${REPO}. \
Check the internet connection on host: ${HOST}" >> ${LOGFILE}
    return 1
  fi
  cmd="wget $wgetParameters ${RPM_GPG_KEY_URL} -O ${tmpDir}/${RPM_GPG_KEY}; rpm --import ${tmpDir}/${RPM_GPG_KEY}"
  eval ssh $sshParameter $HOST \"$cmd\" > /dev/null 2>> ${LOGFILE}
  if [ $? -ne 0 ]
  then
    echo "Can not download the GPG key file: ${RPM_GPG_KEY_URL} on host: ${HOST}" >> ${LOGFILE}
    cmd="rm -f ${tmpDir}/${RPM_GPG_KEY}"
    eval ssh $sshParameter $HOST \"$cmd\" > /dev/null 2>&1

    return 1
  else
    cmd="rm -f ${tmpDir}/${RPM_GPG_KEY}"
    eval ssh $sshParameter $HOST \"$cmd\" > /dev/null 2>&1
  fi
}

function add_repo_yum()
{
  if [ "$1" = "" ]; then return; fi
  if [ "$2" = "" ]; then return; fi
  if [ "$3" = "" ]; then return; fi
  HOST=$1
  REPOFILE=beegfs-$2.repo
  LOGFILE=$3
  REPO=${BASE_REPO_PATH}/${REPOFILE}

  cmd="wget $wgetParameters ${REPO} -O /etc/yum.repos.d/${REPOFILE}"
  eval ssh $sshParameter $HOST \"$cmd\" > /dev/null 2>> ${LOGFILE}
  if [ $? -ne 0 ]
  then
    echo "Can not download the repository configuration file: ${REPO}. \
Check the internet connection on host: ${HOST}" >> ${LOGFILE}
    return 1
  fi
  cmd="wget $wgetParameters ${RPM_GPG_KEY_URL} -O ${tmpDir}/${RPM_GPG_KEY}; rpm --import ${tmpDir}/${RPM_GPG_KEY}"
  eval ssh $sshParameter $HOST \"$cmd\" > /dev/null 2>> ${LOGFILE}
  if [ $? -ne 0 ]
  then
    echo "Can not download the GPG key file: ${RPM_GPG_KEY_URL} on host: ${HOST}" >> ${LOGFILE}
    cmd="rm -f ${tmpDir}/${RPM_GPG_KEY}"
    eval ssh $sshParameter $HOST \"$cmd\" > /dev/null 2>&1

    return 1
  else
    cmd="rm -f ${tmpDir}/${RPM_GPG_KEY}"
    eval ssh $sshParameter $HOST \"$cmd\" > /dev/null 2>&1
  fi
}

function add_repo_apt()
{
  if [ "$1" = "" ]; then return; fi
  if [ "$2" = "" ]; then return; fi
  if [ "$3" = "" ]; then return; fi
  HOST=$1
  REPOFILE=beegfs-$2.list
  LOGFILE=$3
  REPO=${BASE_REPO_PATH}/${REPOFILE}

  cmd="wget $wgetParameters ${REPO} -O /etc/apt/sources.list.d/${REPOFILE}"
  eval ssh $sshParameter $HOST \"$cmd\" > /dev/null 2>> ${LOGFILE}
  if [ $? -ne 0 ]
  then
    echo "Can not download the repository configuration file: ${REPO}. \
Check the internet connection on host: ${HOST}" >> ${LOGFILE}
    return 1
  fi
   cmd="wget $wgetParameters ${DEB_GPG_KEY_URL} -O- | apt-key add -"
  eval ssh $sshParameter $HOST \"$cmd\" > /dev/null 2>> ${LOGFILE}
  if [ $? -ne 0 ]
  then
    echo "Can not download the GPG key file: ${DEB_GPG_KEY_URL} on host: ${HOST}" >> ${LOGFILE}
    return 1
  fi
  cmd="apt-get update"
  eval ssh $sshParameter $HOST \"$cmd\" > /dev/null 2>> ${LOGFILE}
  if [ $? -ne 0 ]
  then
    echo "Update of apt-get cache failed on host: ${HOST}" >> ${LOGFILE}
    return 1
  fi
}



function check_package_manager()
{
  if [ "$1" = "" ]; then return; fi
  HOST=$1

  # yum = 1
  cmd="which yum"
  eval ssh $sshParameter ${HOST} \"$cmd\" > /dev/null 2>&1
  if [ $? -eq 0 ]
  then
    return 1
  fi

  # zypper = 2
  cmd="which zypper"
  eval ssh $sshParameter ${HOST} \"$cmd\" > /dev/null 2>&1
  if [ $? -eq 0 ]
  then
    return 2
  fi

  # unknown = 0
  return 0
}

if [ "$1" = "" ]; then exit 1; else HOST=$1; fi
if [ "$2" = "" ]; then exit 1; else DISTRI=$2; fi
if [ "$3" = "" ]; then exit 1; else FAILED_FILE=$3; fi
if [ "$4" = "" ]; then OUT_FILE=${logFile}; else OUT_FILE=$4; fi

# determine package manager
# if the distri is debian then it's apt otherwise check for zypper or yum
error=1

cmd="mkdir -p ${tmpDir}"
eval ssh $sshParameter ${HOST} \"$cmd\" > /dev/null 2>> ${OUT_FILE}

echo ${DISTRI} | grep "deb" > /dev/null 2>&1
if [ $? -eq 0 ]
then
  # create apt repo
  add_repo_apt ${HOST} ${DISTRI} ${OUT_FILE}
  error=$?
else
  check_package_manager ${HOST}
  retval=$?

  if [ $retval -eq 1 ]
  then
    add_repo_yum ${HOST} ${DISTRI} ${OUT_FILE}
    error=$?
  elif [ $retval -eq 2 ]
  then
    add_repo_zypper ${HOST} ${DISTRI} ${OUT_FILE}
    error=$?
  fi

  if [ $error -ne 0 ]
  then
    echo ${HOST} >> ${FAILED_FILE}
  fi
fi

sleep 1

