#ifndef ASTORAGEPROVIDER_LIB_H
#define ASTORAGEPROVIDER_LIB_H


#if defined(ASTRGE_CORE_LIBRARY)
#  define ASTRGE_LIBRARY_EXPORT Q_DECL_EXPORT
#else
#  define ASTRGE_LIBRARY_EXPORT Q_DECL_IMPORT
#endif

#endif // ASTORAGEPROVIDER_LIB_H