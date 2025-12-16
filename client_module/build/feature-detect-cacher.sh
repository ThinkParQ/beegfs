#!/bin/sh

#
# Feature detect cacher
#
# This speeds up the build by caching the results from kernel feature detection.
# The cache file is reused if the "inputs" still match.
#

set -e

if [ $# -lt 3 ] || [ "$2" != '--' ]
then
   echo >&2 "Usage: feature-detect-cacher.sh <cachedir> -- <feature_test_script> <args...>"
   exit 1
fi

feature_detect_cacher_script=$0
cachedir=$1
feature_detect_script=$3
shift 3

if ! [ -d "$cachedir" ]
then
   echo >&2 "feature-detect-cacher.sh: cache dir '$cachedir' does not exist"
   exit 1
fi

cachefile="$cachedir"/feature-detect.cache
cachekeyfile="$cachedir"/feature-detect.cachekey

# construct cache key
{
   pwd  # kernel source directory
   md5sum "$feature_detect_cacher_script"
   md5sum "$feature_detect_script"

   echo "$@"
   # I'm not sure what environment variables could influence the outcome.
   # Environment can easily change, for example between "make" and "make -j"
   # So let's not include the environment variables in the cache-key for now.
   #env
} > "$cachekeyfile".tmp

cachevalid=false
if cmp -s "$cachekeyfile" "$cachekeyfile".tmp
then
   if [ "$cachefile" -nt "$cachekeyfile" ]
   then
      cachevalid=true
   fi
fi

if "$cachevalid"
then
   echo >&2 "feature-detect-cacher.sh: Using existing feature flags cache in $cachefile"
else
   echo >&2 "feature-detect-cacher.sh: Creating new feature flags cache in $cachefile"

   # run script, constructing cache "value"
   "$feature_detect_script" "$@" > "$cachefile".tmp

   # everything went successful => commit
   mv "$cachefile".tmp "$cachefile"
   mv "$cachekeyfile".tmp "$cachekeyfile"
fi

# output
cat "$cachefile"
