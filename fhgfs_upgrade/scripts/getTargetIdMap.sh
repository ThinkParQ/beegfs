#!/bin/bash

GETTARGETS_CMD="fhgfs-ctl mode=listtargets"
progName=`basename $0`
upgradeMetaName="fhgfs-upgrade-meta-to-2012.10"

printErr()
{
	echo "$@" 1>&2
}


printHelp()
{
	printErr
	printErr "$progName will create a mapping of target string IDs created by the"
	printErr "FhGFS 2011.04 release to numeric IDs required by the 2012.10 release."
	printErr ""
	printErr "Usage: $progName >/path/to/targetIdMap.txt"
	printErr ""
	printErr "Note: The targetIdMap is required by the meta upgrade tool"
	printErr "      ($upgradeMetaName). It *must* be the same file (mapping)"
	printErr "      for all upgrade instances (meta servers) as IDs differently"
	printErr "      mapped for different meta servers will cause an irreparably"
	printErr "      damaged file system!"
	printErr

}

mapIDs()
{
	lineCounter=0
	targetCounter=1
	OLDIFS="$IFS"
	IFS=$'\n'
	TARGETOUT=`eval $GETTARGETS_CMD`
	for line in $TARGETOUT; do
		
		line=`echo $line | sed -e s'/^\s+//'`
		
		col1=`echo $line | awk '{print $1}'`
		col2=`echo $line | awk '{print $2}'`

		if [ $lineCounter -eq 0 -a $col1 == 'TargetID' ]; then
			lineCounter=$(($lineCounter + 1))
			continue;
		elif [ $lineCounter -eq 0 ]; then
			printErr "First output line does not have the expected value: $col1"
			printErr "Check the output of: $GETTARGETS_CMD"
			printErr "Aborting"
			exit 1
		fi

		if [ $lineCounter -eq 1 -a $col1 == '========' ]; then
			lineCounter=$(($lineCounter + 1))
			continue;
		elif [ $lineCounter -eq 1 ]; then
			printErr "First output line does not have the expected value: $col1"
			printErr "Check the output of: $GETTARGETS_CMD"
			printErr "Aborting"
			exit 1
		fi

		echo "$col1      $targetCounter"

		lineCounter=$(($lineCounter + 1))
		targetCounter=$((targetCounter + 1))

	done
	IFS="$OLDIFS"
}

if [ "$1" == "-h" -o "$1" == "--help" ]; then
	printHelp
	exit 1
fi

mapIDs
