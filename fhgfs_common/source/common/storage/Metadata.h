#ifndef METADATA_H_
#define METADATA_H_

#include <common/Common.h>


#define META_ROOTDIR_ID_STR            "root" /* initial file system entry point */
#define META_DISPOSALDIR_ID_STR        "disposal" /* for unlinked but still open files */
#define META_MIRRORDISPOSALDIR_ID_STR  "mdisposal"

#define META_INODES_LEVEL1_SUBDIR_NUM  (128)
#define META_INODES_LEVEL2_SUBDIR_NUM  (128)
#define META_INODES_SUBDIR_NAME        "inodes"     /* contains local file system entry metadata */

#define META_DENTRIES_LEVEL1_SUBDIR_NUM   (128)
#define META_DENTRIES_LEVEL2_SUBDIR_NUM   (128)
#define META_DENTRIES_SUBDIR_NAME         "dentries"   /* contains file system link structure */

#define META_DIRENTRYID_SUB_STR        "#fSiDs#" /* subdir with entryIDs for ID based file access
                                                     * this is a bit dangerous, at it might
                                                     * conflict with user file names, so we need
                                                     * to chose a good name */

#define META_LOSTANDFOUND_PATH         "lost+found"

#define META_BUDDYMIRROR_SUBDIR_NAME   "buddymir"



#endif /*METADATA_H_*/
