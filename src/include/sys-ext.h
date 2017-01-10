// Extension entry point functions:
#if defined (EXT_DLL) && defined(TO_WINDOWS)
    #define EXT_API __declspec(dllexport)
#else
    #define EXT_API extern
#endif

#define MODULE_INIT(m) EXT_API int Init_Module_ ## m (REBVAL* out)
#define MODULE_QUIT(m) EXT_API int Quit_Module_ ## m ()

#define LOAD_MODULE(m) do {         \
    REBVAL out;                     \
    if (!Init_Module_ ## m(&out)) { \
        Add_Boot_Extension(&out);   \
    }                               \
} while(0)

#define UNLOAD_MODULE(m) do {   \
    Quit_Module_ ## m();        \
} while (0)
