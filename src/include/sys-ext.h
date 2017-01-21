// Extension entry point functions:
#if defined (EXT_DLL) && defined(TO_WINDOWS)
    #define EXT_API __declspec(dllexport)
#else
    #define EXT_API extern
#endif

#define MODULE_INIT(m) EXT_API int Module_Init_ ## m (REBVAL* out)
#define CALL_MODULE_INIT_CORE(m) Module_Init_ ## m  ## _Core (out)

#define MODULE_QUIT(m) EXT_API int Module_Quit_ ## m ()
#define CALL_MODULE_QUIT_CORE(m) Module_Quit_ ## m ## _Core ()

#define LOAD_MODULE(exts, m) do {           \
    REBVAL out;                             \
    if (!Module_Init_ ## m(&out)) {         \
        Add_Boot_Extension(exts, &out);     \
    }                                       \
} while(0)

#define UNLOAD_MODULE(m) do {   \
    Module_Quit_ ## m();        \
} while (0)
