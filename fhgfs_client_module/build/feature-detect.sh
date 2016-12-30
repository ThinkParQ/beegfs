#!/bin/bash

set -e

CFLAGS="-D__KERNEL__ $LINUXINCLUDE $KBUILD_CFLAGS $KBUILD_CPPFLAGS -DKBUILD_BASENAME=\"beegfs\""

_check_struct_field_input() {
   local field=$1
   shift 1

   for i in "$@"; do
      echo "#include <$i>"
   done

   cat <<EOF
void want_symbol(void) {
   struct ${field%%::*} s;
   (void) (s.${field##*::});
}
EOF
}

check_struct_field() {
   local field=$1
   local marker=$2
   shift 2

   if _check_struct_field_input "$field" "$@" | $CC $CFLAGS -x c -o /dev/null -c - 2>/dev/null
   then
      echo -D$marker
   fi
}

cd $srctree

check_struct_field \
   address_space_operations::launder_page \
   KERNEL_HAS_LAUNDER_PAGE \
   linux/fs.h

# we have to communicate with the calling makefile somehow. since we can't really use the return
# code of this script, we'll echo a special string at the end of our output for the caller to
# detect and remove again.
# this string has to be something that will, on its own, never be a valid compiler option. so let's
# choose something really, really unlikely like
echo "--~~success~~--"
