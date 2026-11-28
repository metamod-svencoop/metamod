#ifndef SIGNATURE_TEMPLATE_H
#define SIGNATURE_TEMPLATE_H

#define IMPORT_FUNCTION_POINTER_DEFINE(name) fn##name* name = NULL;

#define IMPORT_FUNCTION_DEFINE(name) fn##name name;

#define IMPORT_FUNCTION_EXTERN(name) extern fn##name name;

#define PRIVATE_FUNCTION_DEFINE(name) \
    fn##name g_pfn_##name;            \
    fn##name g_call_original_##name;

#define PRIVATE_FUNCTION_EXTERN(name) \
    extern fn##name g_pfn_##name;     \
    extern fn##name g_call_original_##name;

// Resolve name##_GameSymbol (the gamedata symbolName) in the module at
// dll##Base through metamod's gamedata catalog (interface 5:18). A failure
// logs the catalog status and rejects the plugin.
#define RESOLVE_FROM_GAMEDATA(dll, name, kind, out)                                                                                                                                   \
    do                                                                                                                                                                                \
    {                                                                                                                                                                                 \
        void* name##_GameSymbolAddress = NULL;                                                                                                                                        \
        auto  name##_GameSymbolStatus  = gpMetaUtilFuncs->pfnResolveGameSymbol(dll##Base, name##_GameSymbol, kind, &name##_GameSymbolAddress);                                        \
        if (name##_GameSymbolStatus != META_GAMESYMBOL_OK)                                                                                                                            \
        {                                                                                                                                                                             \
            LOG_ERROR(PLID, "Failed to resolve " #name " (\"%s\") from " #dll " dll: %s", name##_GameSymbol, gpMetaUtilFuncs->pfnGetGameSymbolStatusString(name##_GameSymbolStatus)); \
            return FALSE;                                                                                                                                                             \
        }                                                                                                                                                                             \
        out = (decltype(out))name##_GameSymbolAddress;                                                                                                                                \
    } while (0)

#define FILL_FROM_GAMEDATA(dll, name)                                                  \
    do                                                                                 \
    {                                                                                  \
        RESOLVE_FROM_GAMEDATA(dll, name, META_GAMESYMBOL_KIND_FUNCTION, g_pfn_##name); \
        g_call_original_##name = g_pfn_##name;                                         \
    } while (0)

// A global resolves to the address of the variable itself.
#define VAR_FROM_GAMEDATA(dll, name) RESOLVE_FROM_GAMEDATA(dll, name, META_GAMESYMBOL_KIND_GLOBAL, name)

// Resolve name##_GameSymbol (a virtualFunction gamedata record) to its owning
// vtable slot index in the module at dll##Base.
#define RESOLVE_VFUNC_INDEX_FROM_GAMEDATA(dll, name, out)                                                                                                                                            \
    do                                                                                                                                                                                               \
    {                                                                                                                                                                                                \
        uint32_t name##_GameVFuncIndex  = 0;                                                                                                                                                         \
        auto     name##_GameVFuncStatus = gpMetaUtilFuncs->pfnResolveGameVFuncIndex(dll##Base, name##_GameSymbol, &name##_GameVFuncIndex);                                                           \
        if (name##_GameVFuncStatus != META_GAMESYMBOL_OK)                                                                                                                                            \
        {                                                                                                                                                                                            \
            LOG_ERROR(PLID, "Failed to resolve vfunc index for " #name " (\"%s\") from " #dll " dll: %s", name##_GameSymbol, gpMetaUtilFuncs->pfnGetGameSymbolStatusString(name##_GameVFuncStatus)); \
            return FALSE;                                                                                                                                                                            \
        }                                                                                                                                                                                            \
        out = name##_GameVFuncIndex;                                                                                                                                                                 \
    } while (0)

#ifdef _WIN32

#    define ENGINE_DLL_NAME "hw.dll"

#else

#    ifndef _ARRAYSIZE
#        define _ARRAYSIZE(A) (sizeof(A) / sizeof((A)[0]))
#    endif

#    define ENGINE_DLL_NAME "hw.so"

#endif

#define IMPORT_FUNCTION_DLSYM(dll, name)                                \
    name = (decltype(name))DLSYM((DLHANDLE)dll##Handle, #name);         \
    if (!name)                                                          \
    {                                                                   \
        LOG_ERROR(PLID, "Failed to get " #name " from " #dll " dll !"); \
        return FALSE;                                                   \
    }

#define INSTALL_INLINEHOOK(name) g_phook_##name = gpMetaUtilFuncs->pfnInlineHook((void*)g_pfn_##name, (void*)New##name, (void**)&g_call_original_##name, true)
#define UNINSTALL_HOOK(name)                        \
    if (g_phook_##name)                             \
    {                                               \
        gpMetaUtilFuncs->pfnUnHook(g_phook_##name); \
        g_phook_##name = NULL;                      \
    }

#endif /* SIGNATURE_TEMPLATE_H */
