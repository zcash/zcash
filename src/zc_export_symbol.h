#ifndef ZC_ZC_EXPORT_SYMBOL_H
#define ZC_ZC_EXPORT_SYMBOL_H


/* Exporting a symbol cut-and-pasted from bitcoinconsensus.h
 * This paste and separate header suggests that this approach is too
 * klunky. Let's get rid of globals.
 */

#if defined(BUILD_BITCOIN_INTERNAL) && defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
  #if defined(_WIN32)
    #if defined(DLL_EXPORT)
      #if defined(HAVE_FUNC_ATTRIBUTE_DLLEXPORT)
        #define ZC_EXPORT_SYMBOL __declspec(dllexport)
      #else
        #define ZC_EXPORT_SYMBOL
      #endif
    #endif
  #elif defined(HAVE_FUNC_ATTRIBUTE_VISIBILITY)
    #define ZC_EXPORT_SYMBOL __attribute__ ((visibility ("default")))
  #endif
#elif defined(MSC_VER) && !defined(STATIC_LIBBITCOINCONSENSUS)
  #define ZC_EXPORT_SYMBOL __declspec(dllimport)
#endif

#ifndef ZC_EXPORT_SYMBOL
  #define ZC_EXPORT_SYMBOL
#endif


#endif  // ZC_ZC_EXPORT_SYMBOL_H
