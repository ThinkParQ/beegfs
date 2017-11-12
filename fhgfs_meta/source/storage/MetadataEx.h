#ifndef METADATAEX_H_
#define METADATAEX_H_

#include <common/storage/Metadata.h>
#include <common/Common.h>


#define META_UPDATE_EXT_STR    ".new-fhgfs"

#define META_XATTR_NAME        "user.fhgfs" // attribute name for dir-entries, file and dir metadata

// The size must be sufficient to hold the entire dentry data. In order to simplify various
// operations, meta data or stored into a buffer and for example for a remote directory rename
// operation, this buffer is then transferred over net to the other meta node there used to fill
// the remote dentry, without any knowledge of the actual content.
#define META_SERBUF_SIZE       (1024*8)

#endif /*METADATAEX_H_*/
