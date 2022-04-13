#!/bin/bash

set -e

# NOTE, we're using the argument array as part of this string here. As is
# often the case in shell scripts (especially when used with Makefiles) the
# quoting here is not correct in the sense that it can be broken for example
# with arguments that contain spaces. But it is expected to work with our input
# data.

CFLAGS="-D__KERNEL__ $LINUXINCLUDE $* -DKBUILD_BASENAME=\"beegfs\" -DKBUILD_MODNAME=\"beegfs\""

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

_check_type_input() {
   local tp=$1
   shift 1

   _generate_includes "$@"

   cat <<EOF
void want_tp(void) {
   $tp name;
}
EOF
}

check_type() {
   local tp=$1
   local marker=$2
   shift 2

   _check_type_input "$tp" "$@" | _marker_if_compiles "$marker"
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

_check_struct_field_type_input() {
   local field=$1
   local ftype=$2
   shift 2

   _generate_includes "$@"

   cat <<EOF
void want_symbol(void) {
   struct ${field%%::*} s;
   char predicate[
      2 * __builtin_types_compatible_p(__typeof(s.${field##*::}), $ftype) - 1
   ];
}
EOF
}

check_struct_field_type() {
   local field=$1
   local ftype=$2
   local marker=$3
   shift 3

   _check_struct_field_type_input "$field" "$ftype" "$@" | _marker_if_compiles "$marker"
}

_check_expr_type_input() {
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

check_expr_type() {
   local name=$1
   local signature=$2
   local marker=$3
   shift 3

   _check_expr_type_input "$name" "$signature" "$@" | _marker_if_compiles "$marker"
}

check_function() {
   check_expr_type "$@"
}

_check_symbol_input() {
   local name=$1
   shift

   _generate_includes "$@"
   cat <<EOF
   __typeof__($name) predicate = $name;
EOF
}

check_header() {
   local header=$1
   local marker=$2
   shift 2

   _generate_includes "$header" | _marker_if_compiles "$marker"
}

check_symbol() {
   local name=$1
   local marker=$2
   shift 2

   _check_symbol_input "$name" "$@" | _marker_if_compiles "$marker"
}

check_function \
   generic_readlink "int (struct dentry *, char __user *, int)" \
   KERNEL_HAS_GENERIC_READLINK \
   linux/fs.h

check_header \
   linux/sched/signal.h \
   KERNEL_HAS_SCHED_SIG_H

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

check_function \
   file_dentry "struct dentry* (const struct file *file)" \
   KERNEL_HAS_FILE_DENTRY \
   linux/fs.h

check_function \
   super_setup_bdi_name "int (struct super_block *sb, char *fmt, ...)" \
   KERNEL_HAS_SUPER_SETUP_BDI_NAME \
   linux/fs.h

check_function \
   have_submounts "int (struct dentry *parent)" \
   KERNEL_HAS_HAVE_SUBMOUNTS \
   linux/dcache.h

check_function \
   kernel_read "ssize_t (struct file *, void *, size_t, loff_t *)" \
   KERNEL_HAS_KERNEL_READ \
   linux/fs.h

check_function \
   skwq_has_sleeper "bool (struct socket_wq*)" \
   KERNEL_HAS_SKWQ_HAS_SLEEPER \
   net/sock.h

check_struct_field_type \
   sock::sk_data_ready "void (*)(struct sock*, int)" \
   KERNEL_HAS_SK_DATA_READY_2 \
   net/sock.h

check_struct_field \
   sock::sk_sleep \
   KERNEL_HAS_SK_SLEEP \
   net/sock.h

check_function \
   sk_has_sleeper "int (struct sock*)" \
   KERNEL_HAS_SK_HAS_SLEEPER \
   net/sock.h

check_function \
   __wake_up_sync_key "void (struct wait_queue_head* , unsigned int, void*)" \
   KERNEL_WAKE_UP_SYNC_KEY_HAS_3_ARGUMENTS \
   linux/wait.h

check_expr_type \
  'get_fs()' "mm_segment_t" \
  KERNEL_HAS_GET_FS \
  linux/uaccess.h

check_function \
  iov_iter_kvec 'void (struct iov_iter *i, int direction, const struct kvec *iov, unsigned long nr_segs, size_t count)' \
  KERNEL_HAS_IOV_ITER_KVEC \
  linux/uio.h

# The iov_iter_kvec() function has the same API but differing behaviour
# across Linux versions:
#
# Linux < 4.20 requires ITER_KVEC to be set in "direction"
# Linux >= 4.20 issues a warning (including stacktrace) if it _is_ set.
#
# We can't detect this from the headers, only by looking at the source.
# BUT: The source is not guaranteed to be available when building.
#
# We have tried to check for this "feature" by looking at the Linux version
# as reported by the LINUX_VERSION_CODE macro.
#
# BUT: Some non-vanilla < 4.20 kernels have backported the >= 4.20
# functionality to < 4.20 kernels. So relying on the version code doesn't
# work either.
#
# What we're doing here now is our current best attempt to detect how the API
# must be used. We're checking something that is not really related to the
# actual function, but which seems to give the right results.
check_function \
   iov_iter_is_kvec 'bool(const struct iov_iter *)' \
   KERNEL_HAS_IOV_ITER_KVEC_NO_TYPE_FLAG_IN_DIRECTION \
   linux/uio.h

# print_stack_trace() found on older Linux kernels < 5.2
check_function \
   print_stack_trace 'void (struct stack_trace *trace, int spaces)' \
   KERNEL_HAS_PRINT_STACK_TRACE \
   linux/stacktrace.h

# Starting from kernel 5.2, print_stack_trace() is demoted to tools/ directory
# so not available to us.
# Starting from kernel 5.16 print_stack_trace() is removed completely, but
# stack_trace_print() can be used instead.
# Notably, the identifier stack_trace_print() existed even in older Linux
# versions, but with a different signature and different functionality.
check_function \
   stack_trace_print 'void (unsigned long *trace, unsigned int nr_entries, int spaces)' \
   KERNEL_HAS_STACK_TRACE_PRINT \
   linux/stacktrace.h \

check_type       'struct proc_ops'            KERNEL_HAS_PROC_OPS                     linux/proc_fs.h

check_type       sockptr_t                    KERNEL_HAS_SOCKPTR_T                    linux/sockptr.h

check_function \
   sock_setsockopt 'int (struct socket *, int, int, sockptr_t, unsigned int)' \
   KERNEL_HAS_SOCK_SETSOCKOPT_SOCKPTR_T_PARAM \
   net/sock.h

check_type       time64_t                                                 KERNEL_HAS_TIME64                       linux/ktime.h
check_function   ktime_get_ts64             'void (struct timespec64 *)'  KERNEL_HAS_KTIME_GET_TS64               linux/ktime.h
check_function   ktime_get_real_ts64        'void (struct timespec64 *)'  KERNEL_HAS_KTIME_GET_REAL_TS64          linux/ktime.h
check_function   ktime_get_coarse_real_ts64 'void (struct timespec64 *)'  KERNEL_HAS_KTIME_GET_COARSE_REAL_TS64   linux/ktime.h

# we have to communicate with the calling makefile somehow. since we can't really use the return
# code of this script, we'll echo a special string at the end of our output for the caller to
# detect and remove again.
# this string has to be something that will, on its own, never be a valid compiler option. so let's
# choose something really, really unlikely like
echo "--~~success~~--"
