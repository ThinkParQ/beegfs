#ifndef COMMON_DEBUGVARIABLE_H
#define COMMON_DEBUGVARIABLE_H

#include <boost/lexical_cast.hpp>
#include <stdlib.h>

#if BEEGFS_DEBUG
# define DEBUG_ENV_VAR(type, name, value, envID) \
   static const type name = \
      ::getenv(envID) \
         ? boost::lexical_cast<type>(::getenv(envID)) \
         : value
#else
# define DEBUG_ENV_VAR(type, name, value, envID) \
   static const type name = value
#endif

#endif
