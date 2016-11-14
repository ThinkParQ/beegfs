#!/bin/bash

GETTARGETS_CMD="fhgfs-ctl mode=listnodes nodetype=storage"
progName=`basename $0`
upgradeMetaName="fhgfs-upgrade-meta-to-2012.10"
upgradeStorageName="fhgfs-upgrade-storage-to-2012.10"

printErr()
{
	echo "$@" 1>&2
}


printHelp()
{
	printErr
	printErr "$progName will create a mapping of store string IDs created by the"
	printErr "FhGFS 2011.04 release to numeric IDs required by the 2012.10 release."
	printErr ""
	printErr "Usage: $progName >/path/to/storageIdMap.txt"
	printErr ""
	printErr "Note: The storageIdMap is required by the meta and storage upgrade tools."
	printErr "      ($upgradeMetaName and $upgradeStorageName)."
	printErr "      It *must* be the same file (mapping) for all upgrade instances "
	printErr "      (meta servers and storage targets ) as IDs differently"
	printErr "      mapped for different servers will cause an irreparably"
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
