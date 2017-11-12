#!/bin/bash

set -e

DATE=`date +%F|tr - .`.`date +%T| tr -d :`
dir=`dirname $0`

MAJOR_VER=`${dir}/../beegfs-version --print_major_version`
MINOR_VER=`${dir}/../beegfs-version --print_minor_version`

packages="beegfs_helperd beegfs_meta beegfs_mgmtd beegfs_storage beegfs_utils          \
	beegfs_common_package beegfs_client_devel beeond beeond_thirdparty_gpl"
 
client_packages="beegfs_client_module"
admon_package="beegfs_admon"
opentk_package="beegfs_opentk_lib" # deletes the libopentk.so, so needs to be last

export CONCURRENCY=${MAKE_CONCURRENCY:-4}
PACKAGEDIR="/tmp/beegfs_packages-${DATE}/"

DO_CLEAN=true

LANG=en # make sure language is english, as we grep for certain strings

# print usage information
print_usage()
{
	echo
	echo "USAGE:"
	echo "  $ `basename $0` [options]"
	echo
	echo "OPTIONS:"
        echo "  -v S   Major version string (e.g. \"6\")."
	echo "         Default is based on current date."
        echo "  -m S   Minor version string (e.g. \"2\")."
        echo "         Default is based on current date."
        echo "  -j N   Number of parallel processes for \"make\"."
        echo "  -p S   Package destination directory."
	echo "         Default is a subdir of \"/tmp\"."
	echo "  -c     Run \"make clean\" only."
	echo "  -d     Dry-run, only print export variables."
	echo "  -D     Disable beegfs-admon package build."
	echo "  -C     Build client packages only."
	echo "  -x     Build with BEEGFS_DEBUG."
        echo "  -l F   log to specific file"
        echo "  -K     keep previously built files (no clean)"
        echo "  -o     Use openssl library shipped with beegfs"
        echo "  -s     Use sqlite3 library shipped with beegfs"
        echo "  -u     Use cppunit library shipped with beegfs"
	echo
	echo "EXAMPLE:"
	echo "  $ `basename $0` -j 4 -p /tmp/my_beegfs_packages"
	echo
}

# print given command, execute it and terminate on error return code
run_cmd()
{
	cmd="$@"
	echo "$cmd"
	eval $cmd
	res=$?
	if [ $res -ne 0 ]; then
		echo " failed: $res"
		echo "Aborting"
		exit 1
	fi
}

# parse command line arguments

DRY_RUN=0
CLEAN_ONLY=0
CLIENT_ONLY=0
BUILD_OPENSSL=0
BUILD_CPPUNIT=0
BUILD_SQLITE=0
LOGFILE=

while getopts "hcdm:v:DCxj:p:l:Kosu" opt; do
	case $opt in
	h)
		print_usage
		exit 1
		;;
	c)
		CLEAN_ONLY=1
		;;
	d)
		DRY_RUN=1
		;;
	m)	
		MINOR_VER="$OPTARG"
                ;;
	v)
		MAJOR_VER="$OPTARG"
		;;
	D)
		admon_package=""
		echo "Admon package disabled."
		;;
	C)
		CLIENT_ONLY=1
		;;
	x)
		export BEEGFS_DEBUG=1
		;;
	j)
		export CONCURRENCY="$OPTARG"
		export MAKE_CONCURRENCY="$OPTARG"
		;;
        l)
                if [[ "$OPTARG" == /* ]]; then
                    LOGFILE="$OPTARG"
                else
                    LOGFILE="$PWD/$OPTARG"
                fi
                ;;
	p)
		PACKAGEDIR="$OPTARG"
		;;
        K)
                DO_CLEAN=false
                ;;
        o)      
                BUILD_OPENSSL=1
                ;;
        s)      
                BUILD_SQLITE=1
                ;;
        u)      
                BUILD_CPPUNIT=1
                ;;
        :)
                echo "Option -$OPTARG requires an argument." >&2
                print_usage
                exit 1
                ;;
        *)
                echo "Invalid option: -$OPTARG" >&2
                print_usage
                exit 1
                ;;
   esac
done


if [ $CLIENT_ONLY -eq 0 ]; then
	echo "Building all packages."
	packages="$packages $admon_package $opentk_package $client_packages"
else
	echo "Building client packages only."
	packages=$client_packages
fi

BEEGFS_VERSION="${MAJOR_VER}.${MINOR_VER}"

echo
run_cmd "export BEEGFS_VERSION=$BEEGFS_VERSION"
echo

opentk="beegfs_opentk_lib"
common="beegfs_common"
thirdparty="beegfs_thirdparty"

pushd `dirname $0`/../
ROOT=`pwd`

if [ $DRY_RUN -eq 1 ]; then
	exit 0
fi


# build dependency libs
make_dep_lib()
{
	lib="$1"
	parallel_override="$2"
	targets="$3"

	if [ -n "$parallel_override" ]; then
		make_concurrency=$parallel_override
	else
		make_concurrency=$CONCURRENCY
	fi

	echo "CONCURRENCY: using 'make -j $make_concurrency'"

	echo ${lib}
	pwd
	if ${DO_CLEAN}
        then
                run_cmd "make -C ${lib}/${EXTRA_DIR}/build clean >${LOGFILE:-/dev/null}"
        fi 
	run_cmd "make -C ${lib}/${EXTRA_DIR}/build -j $make_concurrency $targets >${LOGFILE:-/dev/null}"
}

# clean packages up here first, do not do it below, as we need
# common and opentk
if ${DO_CLEAN}
then
   for package in $packages $thirdparty $opentk $common; do
   	echo $package clean
   	make -C ${package}/${EXTRA_DIR}/build clean --silent
   done
fi

if [ ${CLEAN_ONLY} -eq 1 ]; then
	exit 0
fi


# build common, opentk and thirdparty, as others depend on them
if [ $CLIENT_ONLY -eq 0 ]; then
        make_dep_lib $thirdparty "" all # mongoose and ticpp
        if [ ${BUILD_OPENSSL} -eq 1 ]; then
           make_dep_lib $thirdparty "" openssl
        fi
        
        if [ ${BUILD_CPPUNIT} -eq 1 ]; then
           make_dep_lib $thirdparty "" cppunit
        fi
        
        if [ ${BUILD_SQLITE} -eq 1 ]; then
           make_dep_lib $thirdparty "" sqlite
        fi

        make_dep_lib $opentk
        make_dep_lib $common
fi

mkdir -p $PACKAGEDIR
export DEBIAN_ARCHIVE_DIR=$PACKAGEDIR

