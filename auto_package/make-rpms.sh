#!/bin/bash

set -e

dir=`dirname $0`

BEEGFS_VERSION_PATH=$(readlink -f ${dir}/../beegfs-version)
export BEEGFS_VERSION_PATH

source ${dir}/make-packages-common.sh

echo "Writing RPMs to $PACKAGEDIR"

logfile=${LOGFILE:-"/tmp/build_log.$RANDOM"} # SUSE does not have tempfile?
# build rpms here
for package in $packages; do
	echo $package
	#rpm=`make -C ${package}/${EXTRA_DIR}/build rpm 2>&1 | grep "Wrote:" | grep "rpm" | awk '{print $2}'`
	set +e
	run_cmd "make -C ${package}/${EXTRA_DIR}/build rpm 2>&1 | tee $logfile"
	rpm="`cat $logfile | grep "Wrote:" | grep "rpm" | awk '{print $2}'`"
	set -e
	if [ -z "$rpm" ]; then
		echo
		echo "Building the rpm of $package failed! Check $logfile. Aborting!"
		echo
		exit 1
	fi
	mv $rpm ${PACKAGEDIR}/
	rm -fr ${package}/build/buildroot # clean up
done
	
echo "Wrote RPMs to $PACKAGEDIR"
echo

rm -f $logfile

