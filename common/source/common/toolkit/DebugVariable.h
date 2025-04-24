#pragma once

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

