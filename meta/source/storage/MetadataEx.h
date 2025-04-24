#pragma once

#include <common/storage/Metadata.h>
#include <common/Common.h>

#define META_UPDATE_EXT_STR   ".new-fhgfs"
#define META_XATTR_NAME       "user.fhgfs"  // attribute name for dir-entries, file and dir metadata
#define RST_XATTR_NAME        "user.beermt" // attribute name for storing remote storage target info

// !!!IMPORTANT NOTICE TO MAINTAINER!!!
// Any new extended attribute (NON user-defined) that will be added
// in the future MUST be included in the list below.
//
// FAILURE TO DO SO WILL CAUSE:
// Inconsistencies between primary and secondary meta mirrors due to incomplete buddy
// resyncing, as the missing attribute's data will not be resynced to secondary meta.
const std::array<std::string, 2> METADATA_XATTR_NAME_LIST = {META_XATTR_NAME, RST_XATTR_NAME};

// The size must be sufficient to hold the entire dentry data. In order to simplify various
// operations, meta data or stored into a buffer and for example for a remote directory rename
// operation, this buffer is then transferred over net to the other meta node there used to fill
// the remote dentry, without any knowledge of the actual content.
#define META_SERBUF_SIZE       (1024*8)

