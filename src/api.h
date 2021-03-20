#ifndef RISA_API_H_GUARD
#define RISA_API_H_GUARD

#ifdef TARGET_TYPE_LIBRARY
    #if defined _WIN32 || defined __CYGWIN__
        #ifdef __GNUC__
            #define RISA_API __attribute__ ((dllexport))
        #else
            #define RISA_API __declspec(dllexport)
        #endif

        #define RISA_API_HIDDEN
    #else
        #if __GNUC__ >= 4
            #define RISA_API __attribute__ ((visibility ("default")))
            #define RISA_API_HIDDEN  __attribute__ ((visibility ("hidden")))
        #else
            #define RISA_API
            #define RISA_API_HIDDEN
        #endif
    #endif
#else
    #define RISA_API
    #define RISA_API_HIDDEN
#endif

#endif
