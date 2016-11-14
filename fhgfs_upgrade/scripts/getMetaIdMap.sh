#!/bin/bash

GETTARGETS_CMD="fhgfs-ctl mode=listnodes nodetype=meta"
progName=`basename $0`
upgradeMetaName="fhgfs-upgrade-meta-to-2012.10"

printErr()
{
	echo "$@" 1>&2
}


printHelp()
{
	printErr
	printErr "$progName will create a mapping of meta string IDs created by the"
	printErr "FhGFS 2011.04 release to numeric IDs required by the 2012.10 release."
	printErr ""
	printErr "Usage: $progName >/path/to/metaIdMap.txt"
	printErr ""
	printErr "Note: The metaIdMap is required by the meta upgrade tool"
	printErr "      ($upgradeMetaName). It *must* be the same file (mapping)"
	printErr "      for all upgrade instances (meta servers) as IDs differently"
	printErr "      mapped for different meta servers will cause an irreparably"
	printErr "      damaged file system!"
	printErr

}

mapIDs()
{
	targetCounter=1
	OLDIFS="$IFS"
	IFS=$'\n'
	CTL_OUT=`eval $GETTARGETS_CMD`
	if [ $? -ne 0 ]; then
		printErr
		printErr
		printErr "fhgfs-ctl returned an error aborting!"
		printErr
		exit 1
	fi

	for line in $CTL_OUT; do
		
		line=`echo $line | sed -e s'/^\s+//'`
		
		col1=`echo $line | awk '{print $1}'`
		col2=`echo $line | awk '{print $2}'`

		echo "$col1      $targetCounter"

		targetCounter=$((targetCounter + 1))

	done
	IFS="$OLDIFS"
}

if [ "$1" == "-h" -o "$1" == "--help" ]; then
	printHelp
	exit 1
fi

mapIDs
