//
// metamod - cross-architecture (x86 / x64) ABI layout and helper checks
//

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <extdll.h>

#include "meta_api.h"
#include "mm_test_stubs.h"
#include "test_common.h"

namespace
{

char g_stringPool[256]  = {};
int  g_allocStringCalls = 0;

int FakeAllocString(const char* szValue)
{
    ++g_allocStringCalls;
    if (!szValue)
        return 0;

    const std::size_t offset = 64;
    std::strncpy(g_stringPool + offset, szValue, sizeof(g_stringPool) - offset - 1);
    g_stringPool[sizeof(g_stringPool) - 1] = '\0';
    return static_cast<int>(offset);
}

int test_scalar_and_pointer_sizes(void)
{
    TEST("ABI - pointer-sized and fixed-width SDK types");
    static_assert(sizeof(intp) == sizeof(void*), "intp must match pointer size");
    static_assert(sizeof(uintp) == sizeof(void*), "uintp must match pointer size");
    static_assert(sizeof(CRC32_t) == 4, "CRC32_t must be 32-bit on both x86 and x64");
    static_assert(sizeof(string_t) == 4, "string_t must be 32-bit");
    static_assert(sizeof(int32) == 4 && sizeof(uint32) == 4, "int32/uint32 must be 32-bit");
    static_assert(sizeof(int64) == 8 && sizeof(uint64) == 8, "int64/uint64 must be 64-bit");
    static_assert(std::is_same_v<decltype(enginefuncs_t::pfnFunctionFromName), uintptr_t (*)(const char*)>,
                  "pfnFunctionFromName must return uintptr_t");
    static_assert(std::is_same_v<decltype(enginefuncs_t::pfnNameForFunction), const char* (*)(uintptr_t)>,
                  "pfnNameForFunction must accept uintptr_t");
    static_assert(std::is_same_v<decltype(enginefuncs_t::pfnCRC32_Final), CRC32_t (*)(CRC32_t)>,
                  "pfnCRC32_Final must use 32-bit CRC32_t");
    PASS();
    return 0;
}

int test_platform_identifiers(void)
{
    TEST("ABI - PLATFORM_SPC matches build architecture");
#if defined(_WIN64)
    ASSERT_STR(PLATFORM, "mswin");
    ASSERT_STR(PLATFORM_SPC, "win64");
#elif defined(_WIN32)
    ASSERT_STR(PLATFORM, "mswin");
    ASSERT_STR(PLATFORM_SPC, "win32");
#elif defined(__x86_64__) || defined(__amd64__)
    ASSERT_STR(PLATFORM, "linux");
    ASSERT_STR(PLATFORM_SPC, "lin64");
#else
    ASSERT_STR(PLATFORM, "linux");
    ASSERT_STR(PLATFORM_SPC, "lin32");
#endif
    PASS();
    return 0;
}

int test_string_and_make_string(void)
{
    TEST("ABI - STRING / MAKE_STRING behavior");
    globalvars_t globals{};
    globals.pStringBase = g_stringPool;

    globalvars_t* const prevGlobals = gpGlobals;
    auto* const         prevAlloc   = g_engfuncs.pfnAllocString;

    gpGlobals                 = &globals;
    g_engfuncs.pfnAllocString = &FakeAllocString;
    g_allocStringCalls        = 0;

    const char* sample = "worldspawn";
    const auto  handle = MAKE_STRING(sample);
    ASSERT_STR(STRING(handle), "worldspawn");
#if defined(_WIN64) || defined(__x86_64__) || defined(__LP64__)
    ASSERT_INT(g_allocStringCalls, 1);
#else
    ASSERT_INT(g_allocStringCalls, 0);
#endif

    g_engfuncs.pfnAllocString = prevAlloc;
    gpGlobals                 = prevGlobals;
    PASS();
    return 0;
}

} // namespace

int main(void)
{
    int failed = 0;
    failed += test_scalar_and_pointer_sizes();
    failed += test_platform_identifiers();
    failed += test_string_and_make_string();
    return failed ? 1 : 0;
}
