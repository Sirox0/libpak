
#ifndef PAK_EXPORT_H
#define PAK_EXPORT_H

#ifdef PAK_STATIC
#  define PAK_EXPORT
#  define PAK_NO_EXPORT
#else
#  ifndef PAK_EXPORT
#    ifdef pak_EXPORTS
        /* We are building this library */
#      define PAK_EXPORT 
#    else
        /* We are using this library */
#      define PAK_EXPORT 
#    endif
#  endif

#  ifndef PAK_NO_EXPORT
#    define PAK_NO_EXPORT 
#  endif
#endif

#ifndef PAK_DEPRECATED
#  define PAK_DEPRECATED __declspec(deprecated)
#endif

#ifndef PAK_DEPRECATED_EXPORT
#  define PAK_DEPRECATED_EXPORT PAK_EXPORT PAK_DEPRECATED
#endif

#ifndef PAK_DEPRECATED_NO_EXPORT
#  define PAK_DEPRECATED_NO_EXPORT PAK_NO_EXPORT PAK_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef PAK_NO_DEPRECATED
#    define PAK_NO_DEPRECATED
#  endif
#endif

#endif /* PAK_EXPORT_H */
