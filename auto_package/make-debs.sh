#!/bin/bash

set -e

dir=`dirname $0`
source ${dir}/make-packages-common.sh

echo "Writing DEBs to $PACKAGEDIR"

logfile=${LOGFILE:-`mktemp`}
# build debs here
for package in $packages; do
	echo $package
	set +e
	run_cmd "make -C ${package}/${EXTRA_DIR}/build deb 2>&1 | tee $logfile"
	deb=`cat $logfile | grep "Wrote package files"`
	set -e
	if [ -z "$deb" ]; then
		echo
		echo "Building the deb of $package failed! Check $logfile. Aborting!"
		echo
		exit 1
	fi
done

rm -f $PACKAGEDIR/*.{build,changes,dsc,tar.gz}
	
echo "Wrote DEBs to $PACKAGEDIR"
echo

rm -f $logfile
