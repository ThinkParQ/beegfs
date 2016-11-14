#!/bin/bash

# Print the build type suitable to be used for autoconf
# we need this for cppunit and tilera architechture

# ./configure --build the-build-type

# on success print the build type and exit
print_build_type()
{
	cmd="$@"
	build_type=`eval $cmd 2>/dev/null`
	if [ $? -eq 0 ]; then
		echo --build $build_type
		exit 0
	fi
	
}

# try rpm first
print_build_type "rpm --eval %{_host}"

# rpm failed, now dpkg
print_build_type "dpkg-architecture -qDEB_BUILD_GNU_TYPE"


# unknown build type
exit 1
