/*
 * FsckException.h
 *
 *      This exception is intended for things that can happen in fsck, that prevents it from running
 *      further,  e.g. if a server goes down fsck should abort
 *
 */

#ifndef FSCKEXCEPTION_H
#define FSCKEXCEPTION_H

#include <common/toolkit/NamedException.h>

DECLARE_NAMEDEXCEPTION(FsckException, "FsckException")

#endif /* FSCKEXCEPTION_H */
