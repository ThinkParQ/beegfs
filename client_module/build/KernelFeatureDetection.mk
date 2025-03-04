# All detected features are included in "KERNEL_FEATURE_DETECTION"

# parameters:
# $1: name to define when grep finds something
# $2: grep flags and expression
# $3: input files in linux source tree
define define_if_matches
$(eval \
   KERNEL_FEATURE_DETECTION += $$(shell \
      grep -q -s $2 $(addprefix ${KSRCDIR_PRUNED_HEAD}/include/linux/,$3) \
         && echo "-D$(strip $1)"))
endef

ifneq ($(OFED_INCLUDE_PATH),)
OFED_DETECTION_PATH := $(OFED_INCLUDE_PATH)
else
OFED_DETECTION_PATH := ${KSRCDIR_PRUNED_HEAD}/include
endif


# Find out whether rdma_create_id function has qp_type argument.
# This is tricky because the function declaration spans multiple lines.
# Note: Was introduced in vanilla 3.0
KERNEL_FEATURE_DETECTION += $(shell \
   grep -sA2 "struct rdma_cm_id \*rdma_create_id(" ${OFED_DETECTION_PATH}/rdma/rdma_cm.h 2>&1 \
      | grep -qs "ib_qp_type qp_type);" \
      && echo "-DOFED_HAS_RDMA_CREATE_QPTYPE")

# Find out whether rdma_set_service_type function has been declared.
# Note: Was introduced in vanilla 2.6.24
KERNEL_FEATURE_DETECTION += $(shell \
   grep -qs "rdma_set_service_type(struct rdma_cm_id \*id, int tos)" \
      ${OFED_DETECTION_PATH}/rdma/rdma_cm.h \
      && echo "-DOFED_HAS_SET_SERVICE_TYPE")

# Find out whether ib_create_cq function has cq_attr argument
# This is tricky because the function declaration spans multiple lines.
# Note: Was introduced in vanilla 4.2
KERNEL_FEATURE_DETECTION += $(shell \
   grep -sA4 "struct ib_cq \*ib_create_cq(struct ib_device \*device," ${OFED_DETECTION_PATH}/rdma/ib_verbs.h 2>&1 \
      | grep -qs "const struct ib_cq_init_attr \*cq_attr);" \
      && echo "-DOFED_HAS_IB_CREATE_CQATTR")

# Find out whether rdma_reject function has reason argument
# This is tricky because the function declaration spans multiple lines.
# Note: Was introduced in MLNX OFED 5.1
KERNEL_FEATURE_DETECTION += $(shell \
   grep -sA1 "int rdma_reject(" ${OFED_DETECTION_PATH}/rdma/rdma_cm.h 2>&1 \
      | grep -qs "u8 reason);" \
      && echo "-DOFED_RDMA_REJECT_NEEDS_REASON")

# kernels >=v4.4 expect a netns argument for rdma_create_id
KERNEL_FEATURE_DETECTION += $(shell \
   grep -qs "struct rdma_cm_id \*rdma_create_id.struct net \*net," \
      ${OFED_DETECTION_PATH}/rdma/rdma_cm.h \
      && echo "-DOFED_HAS_NETNS")

# kernels >=v4.4 split up ib_send_wr into a lot of other structs
KERNEL_FEATURE_DETECTION += $(shell \
   grep -qs -F "struct ib_atomic_wr {" \
      ${OFED_DETECTION_PATH}/rdma/ib_verbs.h \
      && echo "-DOFED_SPLIT_WR")

KERNEL_FEATURE_DETECTION += $(shell \
   grep -qs -F "IB_PD_UNSAFE_GLOBAL_RKEY" \
      ${OFED_DETECTION_PATH}/rdma/ib_verbs.h \
      && echo "-DOFED_UNSAFE_GLOBAL_RKEY")

KERNEL_FEATURE_DETECTION += $(shell \
   grep -qs -F "ib_get_dma_mr" \
      ${OFED_DETECTION_PATH}/rdma/ib_verbs.h \
      && echo "-DOFED_IB_GET_DMA_MR")

KERNEL_FEATURE_DETECTION += $(shell \
   grep -qs -F "static inline void ib_destroy_cq" \
      ${OFED_DETECTION_PATH}/rdma/ib_verbs.h \
      && echo "-DOFED_IB_DESTROY_CQ_IS_VOID")

# Find out whether the kernel has a scsi/fc_compat.h file, which defines
# vlan_dev_vlan_id.
# Note: We need this, because some kernels (e.g. RHEL 5.9's 2.6.18) forgot this
# include in their rdma headers, leading to implicit function declarations.
$(call define_if_matches, KERNEL_HAS_SCSI_FC_COMPAT, "vlan_dev_vlan_id", scsi/fc_compat.h)

# Find out whether the kernel has a ihold function.
# Note: Was added in vanilla 2.6.37, but RedHat adds it to their 2.6.32.
$(call define_if_matches, KERNEL_HAS_IHOLD, ' ihold(struct inode.*)', fs.h)

# Find out whether the kernel has fsync start and end range arguments.
# Note: fsync start and end were added in vanilla 3.1, but SLES11SP3 adds it to its 3.0 kernel.
$(call define_if_matches, KERNEL_HAS_FSYNC_RANGE, \
   -F "int (*fsync) (struct file *, loff_t, loff_t, int datasync);", fs.h)

# Find out whether the kernel has define struct dentry_operations *s_d_op
# in struct super_block. If it has it used that to check if the file system
# needs to revalidate dentries.
$(call define_if_matches, KERNEL_HAS_S_D_OP, -F "struct dentry_operations *s_d_op;", fs.h)

# Find out whether the kernel has d_materialise_unique() to
# add dir dentries.
#
# Note: d_materialise_unique was added in vanilla 2.6.19 (backported to rhel5
# 2.6.18) and got merged into d_splice_alias in vanilla 3.19.
$(call define_if_matches, KERNEL_HAS_D_MATERIALISE_UNIQUE, \
   -F "d_materialise_unique(struct dentry *, struct inode *)", dcache.h)

# Find out whether the kernel has a PDE_DATA method.
#
# Note: This method was added in vanilla linux-3.10
$(call define_if_matches, KERNEL_HAS_PDE_DATA, -F "PDE_DATA(const struct inode *)", proc_fs.h)

# Find out whether the kernel has i_uid_read
#
# Note: added to 3.5
$(call define_if_matches, KERNEL_HAS_I_UID_READ, "i_uid_read", fs.h)

# Find out whether the kernel has atomic_open
#
# Note: added to 3.5
$(call define_if_matches, KERNEL_HAS_ATOMIC_OPEN, "atomic_open", fs.h)

# Find out whether the kernel used umode_t
#
# Note: added to 3.3
$(call define_if_matches, KERNEL_HAS_UMODE_T, -E "(\*mkdir).*umode_t", fs.h)

# Find out if the kernel has a file_inode() method.
$(call define_if_matches, KERNEL_HAS_FILE_INODE, " file_inode(.*)", fs.h)

# Find out whether the kernel has a strnicmp function.
#
# Note: strnicmp was switched to strncasecmp in linux-4.0. strncasecmp existed
# before, but was wrong, so we only use strncasecmp if strnicmp doesn't exist.
$(call define_if_matches, KERNEL_HAS_STRNICMP, "strnicmp", string.h)

# Find out whether the kernel has BDI_CAP_MAP_COPY defined.
$(call define_if_matches, KERNEL_HAS_BDI_CAP_MAP_COPY, "define BDI_CAP_MAP_COPY", backing-dev.h)

# Find out whether xattr_handler** s_xattr in super_block is const.
$(call define_if_matches, KERNEL_HAS_CONST_XATTR_CONST_PTR_HANDLER, \
   -F "const struct xattr_handler * const *s_xattr;", fs.h)

$(call define_if_matches, KERNEL_HAS_CONST_XATTR_HANDLER, \
   -F "const struct xattr_handler **s_xattr;", fs.h)

# Find out whether xattr_handler functions need a dentry* (otherwise they need an inode*).
# Note: grepping for "(*set).struct..." instead of "(*set)(struct..." because make complains about
#       the missing ")" otherwise.
$(call define_if_matches, KERNEL_HAS_DENTRY_XATTR_HANDLER, \
   "int (\*set).struct dentry \*dentry", xattr.h)

# address_space.assoc_mapping went away in vanilla 3.8, but SLES11 backports that change
$(call define_if_matches, KERNEL_HAS_ADDRSPACE_ASSOC_MAPPING, -F "assoc_mapping", fs.h)

# current_umask() was added in 2.6.30
$(call define_if_matches, KERNEL_HAS_CURRENT_UMASK, -F "current_umask", fs.h)

# super_operations.show_options was changed to struct dentry* in 3.3
$(call define_if_matches, KERNEL_HAS_SHOW_OPTIONS_DENTRY, -F "int (*show_options)(struct seq_file *, struct dentry *);", fs.h)

# xattr handlers >=v4.4 also receive pointer to struct xattr_handler
KERNEL_FEATURE_DETECTION += $(shell \
   grep -s -F "int (*get)" ${KSRCDIR_PRUNED_HEAD}/include/linux/xattr.h \
      | grep -q -s -F "const struct xattr_handler *" \
      && echo "-DKERNEL_HAS_XATTR_HANDLER_PTR_ARG -DKERNEL_HAS_DENTRY_XATTR_HANDLER")

# 4.5 introduces name in xattr_handler, which can be used instead of prefix
KERNEL_FEATURE_DETECTION += $(shell \
   grep -sFA1 "struct xattr_handler {" ${KSRCDIR_PRUNED_HEAD}/include/linux/xattr.h \
      | grep -qsF "const char *name;" \
      && echo "-DKERNEL_HAS_XATTR_HANDLER_NAME")

# locks_lock_inode_wait is used for flock since 4.4 (before flock_lock_file_wait was used)
# since 6.3 locks_lock_inode_wait moved from file fs.h to filelock.h
$(call define_if_matches, KERNEL_HAS_LOCKS_FILELOCK_INODE_WAIT, -F "static inline int locks_lock_inode_wait(struct inode *inode, struct file_lock *fl)", filelock.h)

$(call define_if_matches, KERNEL_HAS_LOCKS_LOCK_INODE_WAIT, -F "static inline int locks_lock_inode_wait(struct inode *inode, struct file_lock *fl)", fs.h)

# get_link() replaces follow_link() in 4.5
$(call define_if_matches, KERNEL_HAS_GET_LINK, -F "const char * (*get_link) (struct dentry *, struct inode *, struct delayed_call *);", fs.h)

$(call define_if_matches, KERNEL_HAS_I_MMAP_LOCK, -F "i_mmap_lock_read", fs.h)
$(call define_if_matches, KERNEL_HAS_I_MMAP_RWSEM, -F "i_mmap_rwsem", fs.h)
$(call define_if_matches, KERNEL_HAS_I_MMAP_MUTEX, -F "i_mmap_mutex", fs.h)
$(call define_if_matches, KERNEL_HAS_I_MMAP_RBTREE, -P "struct rb_root\s+i_mmap", fs.h)
$(call define_if_matches, KERNEL_HAS_I_MMAP_CACHED_RBTREE, -P "struct rb_root_cached\s+i_mmap", fs.h)
$(call define_if_matches, KERNEL_HAS_I_MMAP_NONLINEAR, -F "i_mmap_nonlinear", fs.h)

$(call define_if_matches, KERNEL_HAS_INODE_LOCK, "static inline void inode_lock", fs.h)

#<linuy-4.8
$(call define_if_matches, KERNEL_HAS_PAGE_ENDIO, \
   -F "void page_endio(struct page *page, int rw, int err);", pagemap.h)
#>=linux-4.8
$(call define_if_matches, KERNEL_HAS_PAGE_ENDIO, \
   -F "void page_endio(struct page *page, bool is_write, int err);", pagemap.h)

# kernels <= 2.7.27 use remove_suid, others use file_remove_suid.
# except linux-3.10.0-514.el7, which uses file_remove_privs.
$(call define_if_matches, KERNEL_HAS_FILE_REMOVE_SUID, \
   -F "int file_remove_suid(struct file *);", fs.h)
$(call define_if_matches, KERNEL_HAS_FILE_REMOVE_PRIVS, \
   -F "int file_remove_privs(struct file *);", fs.h)

KERNEL_FEATURE_DETECTION += $(shell \
   grep -sFA1 "sock_recvmsg" ${KSRCDIR_PRUNED_HEAD}/include/linux/net.h \
      | grep -qsF "size_t size" \
      && echo "-DKERNEL_HAS_RECVMSG_SIZE")

$(call define_if_matches, KERNEL_HAS_MEMDUP_USER, "memdup_user", string.h)
$(call define_if_matches, KERNEL_HAS_FAULTATTR_DNAME, -F "struct dentry *dname", fault-inject.h)
$(call define_if_matches, KERNEL_HAS_SYSTEM_UTSNAME, "system_utsname", utsname.h)
$(call define_if_matches, KERNEL_HAS_SOCK_CREATE_KERN_NS, "sock_create_kern.struct net", net.h)
$(call define_if_matches, KERNEL_HAS_SOCK_SENDMSG_NOLEN, "sock_sendmsg.*msg.;", net.h)
$(call define_if_matches, KERNEL_HAS_IOV_ITER_INIT_DIR, "iov_iter_init.*direction", uio.h)
$(call define_if_matches, KERNEL_HAS_ITER_KVEC, "ITER_KVEC", uio.h)
$(call define_if_matches, KERNEL_HAS_IOV_ITER_TYPE, "iov_iter_type", uio.h)
$(call define_if_matches, KERNEL_HAS_ITER_BVEC, "ITER_BVEC", uio.h)
$(call define_if_matches, KERNEL_HAS_ITER_PIPE, "ITER_PIPE", uio.h)
$(call define_if_matches, KERNEL_HAS_IOV_ITER_IS_PIPE, "iov_iter_is_pipe", uio.h)
$(call define_if_matches, KERNEL_HAS_ITER_IS_IOVEC, "iter_is_iovec", uio.h)
$(call define_if_matches, KERNEL_HAS_IOV_ITER_IOVEC, "iov_iter_iovec", uio.h)
# iov_iter_iovec removed from 6.4 kernel and used iter_iov_addr & iter_iov_len macro's
$(call define_if_matches, KERNEL_HAS_ITER_IOV_ADDR, "iter_iov_addr", uio.h)
$(call define_if_matches, KERNEL_HAS_GET_SB_NODEV, "get_sb_nodev", fs.h)
$(call define_if_matches, KERNEL_HAS_GENERIC_FILE_LLSEEK_UNLOCKED, "generic_file_llseek_unlocked", \
   fs.h)
$(call define_if_matches, KERNEL_HAS_SET_NLINK, "set_nlink", fs.h)
$(call define_if_matches, KERNEL_HAS_DENTRY_PATH_RAW, "dentry_path_raw", dcache.h)
$(call define_if_matches, KERNEL_HAS_FSYNC_DENTRY, -P "(\*fsync).*dentry", fs.h)
$(call define_if_matches, KERNEL_HAS_ITER_FILE_SPLICE_WRITE, "iter_file_splice_write", fs.h)
$(call define_if_matches, KERNEL_HAS_ITER_GENERIC_FILE_SENDFILE, "generic_file_sendfile", fs.h)
$(call define_if_matches, KERNEL_HAS_ITERATE_DIR, "iterate_dir", fs.h)
$(call define_if_matches, KERNEL_HAS_ENCODE_FH_INODE, -P "\(\*encode_fh\).struct inode", exportfs.h)
$(call define_if_matches, KERNEL_HAS_D_DELETE_CONST_ARG, \
   -F "int (*d_delete)(const struct dentry *);", dcache.h)
$(call define_if_matches, KERNEL_HAS_FILE_F_VFSMNT, -P "struct vfsmount\s*\*f_vfsmnt", fs.h)
$(call define_if_matches, KERNEL_HAS_POSIX_ACL_XATTR_USERNS_ARG, \
   -P "posix_acl_from_xattr.struct user_namespace", posix_acl_xattr.h)
$(call define_if_matches, KERNEL_HAS_D_MAKE_ROOT, d_make_root, dcache.h)
$(call define_if_matches, KERNEL_HAS_GENERIC_WRITE_CHECKS_ITER, \
   -P "generic_write_checks.*iov_iter", fs.h)
$(call define_if_matches, KERNEL_HAS_INVALIDATEPAGE_RANGE, \
   -P "void \(\*invalidatepage\) \(struct page \*. unsigned int. unsigned int\);", fs.h)
$(call define_if_matches, KERNEL_HAS_PERMISSION_2, \
   -P "int \(\*permission\) \(struct inode \*. int\);", fs.h)
$(call define_if_matches, KERNEL_HAS_PERMISSION_FLAGS, \
   -P "int \(\*permission\) \(struct inode \*. int. unsigned int\);", fs.h)
KERNEL_FEATURE_DETECTION += $(shell \
   grep -sFA5 "kmem_cache_create" ${KSRCDIR_PRUNED_HEAD}/include/linux/slab.h \
      | grep -qsF "void (*)(void *, struct kmem_cache *, unsigned long)" \
      && echo "-DKERNEL_HAS_KMEMCACHE_CACHE_FLAGS_CTOR")
KERNEL_FEATURE_DETECTION += $(shell \
   grep -sFA5 "kmem_cache_create" ${KSRCDIR_PRUNED_HEAD}/include/linux/slab.h \
      | grep -qsF "void (*)(struct kmem_cache *, void *)" \
      && echo "-DKERNEL_HAS_KMEMCACHE_CACHE_CTOR")
KERNEL_FEATURE_DETECTION += $(shell \
   grep -sFA5 "kmem_cache_create" ${KSRCDIR_PRUNED_HEAD}/include/linux/slab.h \
      | grep -qsxP "\s+void \(\*\)\(.*?\)," \
      && echo "-DKERNEL_HAS_KMEMCACHE_DTOR")
$(call define_if_matches, KERNEL_HAS_SB_BDI, -F "struct backing_dev_info *s_bdi", fs.h)
$(call define_if_matches, KERNEL_HAS_BDI_SETUP_AND_REGISTER, "bdi_setup_and_register", \
   backing-dev.h)
$(call define_if_matches, KERNEL_HAS_FOLLOW_LINK_COOKIE, \
   -P "const char \* \(\*follow_link\) \(struct dentry \*. void \*\*\);", fs.h)
$(call define_if_matches, KERNEL_HAS_FSYNC_2, \
   -F "int (*fsync) (struct file *, int datasync);", fs.h)
KERNEL_FEATURE_DETECTION += $(shell \
   grep -sFA20 "struct address_space {" ${KSRCDIR_PRUNED_HEAD}/include/linux/fs.h \
      | grep -qsP "struct backing_dev_info *backing_dev_info;" \
      && echo "-DKERNEL_HAS_ADDRESS_SPACE_BDI")
$(call define_if_matches, KERNEL_HAS_COPY_FROM_ITER, "copy_from_iter", uio.h)
$(call define_if_matches, KERNEL_HAS_ALLOC_WORKQUEUE, "alloc_workqueue", workqueue.h)
$(call define_if_matches, KERNEL_HAS_WQ_RESCUER, "WQ_RESCUER", workqueue.h)
$(call define_if_matches, KERNEL_HAS_WAIT_QUEUE_ENTRY_T, "wait_queue_entry_t", wait.h)
$(call define_if_matches, KERNEL_HAS_CURRENT_FS_TIME, "current_fs_time", fs.h)
$(call define_if_matches, KERNEL_HAS_64BIT_TIMESTAMPS, "struct timespec64 ia_atime;", fs.h)
$(call define_if_matches, KERNEL_HAS_SB_NODIRATIME, "SB_NODIRATIME", fs.h)

$(call define_if_matches, KERNEL_HAS_GENERIC_GETXATTR, "generic_getxattr", xattr.h)

KERNEL_FEATURE_DETECTION += $(shell \
   grep -sA1 "(*rename) " $(KSRCDIR_PRUNED_HEAD)/include/linux/fs.h \
      | grep -qsF "unsigned int" \
      && echo "-DKERNEL_HAS_RENAME_FLAGS")

KERNEL_FEATURE_DETECTION += $(shell \
   grep -qsF "static inline ino_t parent_ino" $(KSRCDIR_PRUNED_HEAD)/include/linux/fs.h \
   && echo "-DKERNEL_HAS_PARENT_INO")

KERNEL_FEATURE_DETECTION += $(shell \
   grep -qsF "define SLAB_MEM_SPREAD" $(KSRCDIR_PRUNED_HEAD)/include/linux/slab.h \
   && echo "-DKERNEL_HAS_SLAB_MEM_SPREAD")

$(call define_if_matches, KERNEL_ACCESS_OK_WANTS_TYPE, "define access_ok(type, addr, size)" \
   $(KSRCDIR_PRUNED_HEAD)/include/asm-generic/uaccess.h)

$(call define_if_matches, KERNEL_SPIN_RELEASE_HAS_3_ARGUMENTS, "\#define spin_release(l, n, i)", lockdep.h)

$(call define_if_matches, KERNEL_HAS_NEW_PDE_DATA, "pde_data", proc_fs.h)

# From linux-5.18, .readpages, .invalidatepage, .set_page_dirty, .launder_page
# are replaced by.readahead, .invalidate_folio, .dirty_folio, launder_folio
#$(call define_if_matches, KERNEL_HAS_READAHEAD, -F "void (*readahead)", fs.h)
$(call define_if_matches, KERNEL_HAS_FOLIO, -F "bool (*dirty_folio)", fs.h)

$(call define_if_matches, KERNEL_HAS_READ_FOLIO, -F "int (*read_folio)", fs.h)

KERNEL_FEATURE_DETECTION += $(shell \
    grep -sFA1 "int (*writepage_t)" ${KSRCDIR_PRUNED_HEAD}/include/linux/writeback.h \
    | grep -qsF "struct folio *" \
    && echo "-DKERNEL_WRITEPAGE_HAS_FOLIO")

KERNEL_FEATURE_DETECTION += $(shell \
    grep -sFA1 "int (*write_begin)" ${KSRCDIR_PRUNED_HEAD}/include/linux/fs.h \
      | grep -qsF "unsigned flags" \
      && echo "-DKERNEL_WRITE_BEGIN_HAS_FLAGS")

# Matching: int posix_acl_chmod(struct user_namespace *, struct dentry *, umode_t)
KERNEL_FEATURE_DETECTION += $(shell \
    grep -sF "int posix_acl_chmod" ${KSRCDIR_PRUNED_HEAD}/include/linux/posix_acl.h \
    | grep -qs "struct user_namespace\s*.*struct dentry" \
    && echo "-DKERNEL_HAS_POSIX_ACL_CHMOD_NS_DENTRY")

# linux-6.0 has iov_iter_get_pages2
$(call define_if_matches, KERNEL_HAS_IOV_ITER_GET_PAGES2, "iov_iter_get_pages2", uio.h)

$(call define_if_matches, KERNEL_HAS_GET_RANDOM_INT, "get_random_int", random.h)

