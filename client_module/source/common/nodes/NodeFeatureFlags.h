#ifndef NODEFEATUREFLAGS_H_
#define NODEFEATUREFLAGS_H_

/*
 * Remember to keep this in sync with userspace common's NodeFeatureFlags.h file
 *
 * The numbers defined here are feature bit indices, so it's "0, 1, 2, 3, ..."
 * (not "1, 2, 4, 8, ...").
 */


/**
 *  max supported feature flag bit index (for pre-alloced BitStore size in node objects).
 *
 *  note: a 32 bit block is the smallest unit that is alloc'ed anyways on 32bit systems.
 */
#define NODE_FEATURES_MAX_INDEX    32


// mgmtd feature flags

#define MGMT_FEATURE_DUMMY          0


// meta feature flags

#define META_FEATURE_DUMMY          0


// storage feature flags

#define STORAGE_FEATURE_DUMMY       0


// client feature flags

#define CLIENT_FEATURE_DUMMY        0


#endif /* NODEFEATUREFLAGS_H_ */
