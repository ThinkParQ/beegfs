#!/bin/bash

set -e

CFLAGS="-D__KERNEL__ $LINUXINCLUDE $KBUILD_CFLAGS $KBUILD_CPPFLAGS -DKBUILD_BASENAME=\"beegfs\""

_generate_includes() {
   for i in "$@"; do
      echo "#include <$i>"
   done
}

_marker_if_compiles() {
   local marker=$1
   shift

   if $CC $CFLAGS -x c -o /dev/null -c - 2>/dev/null
   then
      echo -D$marker
   fi
}

_check_struct_field_input() {
   local field=$1
   shift 1

   _generate_includes "$@"

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

   _check_struct_field_input "$field" "$@" | _marker_if_compiles "$marker"
}

_check_function_input() {
   local name=$1
   local signature=$2
   shift 2

   _generate_includes "$@"
   cat <<EOF
void want_fn(void) {
   char predicate[
      2 * __builtin_types_compatible_p(__typeof($name), $signature) - 1
   ];
}
EOF
}

check_function() {
   local name=$1
   local signature=$2
   local marker=$3
   shift 3

   _check_function_input "$name" "$signature" "$@" | _marker_if_compiles "$marker"
}

check_header() {
   local header=$1
   local marker=$2
   shift 2

   _generate_includes "$header" | _marker_if_compiles "$marker"
}

check_struct_field \
   address_space_operations::launder_page \
   KERNEL_HAS_LAUNDER_PAGE \
   linux/fs.h

check_function \
   generic_readlink "int (struct dentry *, char __user *, int)" \
   KERNEL_HAS_GENERIC_READLINK \
   linux/fs.h

check_header \
   linux/sched/signal.h \
   KERNEL_HAS_SCHED_SIG_H

# older kernels (RHEL5) don't have linux/cred.h
check_header \
   linux/cred.h \
   KERNEL_HAS_CRED_H

# cryptohash does not include linux/types.h, so the type comparison fails
check_function \
   half_md4_transform "__u32 (__u32[4], __u32 const in[8])" \
   KERNEL_HAS_HALF_MD4_TRANSFORM \
   linux/types.h linux/cryptohash.h

check_function \
   vfs_getattr "int (const struct path *, struct kstat *, u32, unsigned int)" \
   KERNEL_HAS_STATX \
   linux/fs.h

check_function \
   kref_read "unsigned int (const struct kref*)" \
   KERNEL_HAS_KREF_READ \
   linux/kref.h

# we have to communicate with the calling makefile somehow. since we can't really use the return
# code of this script, we'll echo a special string at the end of our output for the caller to
# detect and remove again.
# this string has to be something that will, on its own, never be a valid compiler option. so let's
# choose something really, really unlikely like
echo "--~~success~~--"
