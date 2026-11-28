//
// metamod - regression tests for MPlugin::ini_parseline()
//
// Covers the plugins.ini line parser: the trimmed line buffer must be freed
// through the original allocation, and trimming must not form a pointer before
// the start of that buffer when the line is empty or whitespace-only.
// Built with AddressSanitizer on Linux, both defects abort the process.
//

#include <filesystem>
#include <fstream>
#include <string>
#include <string.h>

#include <extdll.h>

#include "mplugin.h"
#include "metamod.h"

#include "mm_test_stubs.h"
#include "test_common.h"

static void make_plugin(MPlugin* plug)
{
    memset(plug, 0, sizeof(*plug));
    plug->index = 1;
}

// An empty line carries nothing but the terminator.
static int test_ini_parseline_empty_line(void)
{
    MPlugin plug;
    make_plugin(&plug);

    TEST("ini_parseline - empty line reports ME_BLANK");
    ASSERT_FALSE(plug.ini_parseline(""));
    ASSERT_INT(meta_errno, ME_BLANK);
    PASS();
    return 0;
}

// Whitespace-only lines are the case that used to index one byte before the
// strdup() buffer while stripping trailing whitespace.
static int test_ini_parseline_whitespace_only(void)
{
    static const char* const lines[] = {
        " ",
        "   ",
        "\t",
        "\t \t ",
        " \t\t ",
    };

    for (unsigned int i = 0; i < sizeof(lines) / sizeof(lines[0]); i++)
    {
        MPlugin plug;
        char    msg[64];
        make_plugin(&plug);

        snprintf(msg, sizeof(msg), "ini_parseline - whitespace-only line #%u", i);
        TEST(msg);
        ASSERT_FALSE(plug.ini_parseline(lines[i]));
        ASSERT_INT(meta_errno, ME_BLANK);
        PASS();
    }
    return 0;
}

// Surrounding whitespace is stripped, the parsed fields are the trimmed ones.
static int test_ini_parseline_trims_whitespace(void)
{
    MPlugin plug;
    char    line[256];
    make_plugin(&plug);

    TEST("ini_parseline - leading/trailing whitespace is trimmed");
    snprintf(line, sizeof(line), "  %s   dlls/mm_test_i386.so\tTest Plugin\t  ",
             PLATFORM_SPC);
    ASSERT_TRUE(plug.ini_parseline(line));
    ASSERT_STR(plug.filename, "dlls/mm_test_i386.so");
    ASSERT_STR(plug.file, "mm_test_i386.so");
    ASSERT_STR(plug.desc, "Test Plugin");
    ASSERT_INT(plug.status, PL_VALID);
    ASSERT_INT(plug.source, PS_INI);
    PASS();
    return 0;
}

// Trailing whitespace with no description: the description falls back to the
// bare file name, which also proves the trim stopped at the right place.
static int test_ini_parseline_trailing_whitespace_only(void)
{
    MPlugin plug;
    char    line[256];
    make_plugin(&plug);

    TEST("ini_parseline - trailing whitespace without description");
    snprintf(line, sizeof(line), "%s mm_test_i386.so   \t ", PLATFORM_SPC);
    ASSERT_TRUE(plug.ini_parseline(line));
    ASSERT_STR(plug.filename, "mm_test_i386.so");
    ASSERT_STR(plug.desc, "<mm_test_i386.so>");
    PASS();
    return 0;
}

// The file pointer addresses storage inside the plugin, not the caller's line.
static int test_ini_parseline_file_points_into_plugin(void)
{
    MPlugin plug;
    char    line[256];
    make_plugin(&plug);

    TEST("ini_parseline - file points into the copied filename");
    snprintf(line, sizeof(line), "%s dlls/mm_test_i386.so Test Plugin", PLATFORM_SPC);
    ASSERT_TRUE(plug.ini_parseline(line));
    ASSERT_PTR_EQ(plug.file, plug.filename + strlen("dlls/"));
    PASS();
    return 0;
}

static int test_ini_parseline_comments(void)
{
    static const char* const lines[] = {
        "#comment",
        "; comment",
        "// comment",
        "#",
        ";",
    };

    for (unsigned int i = 0; i < sizeof(lines) / sizeof(lines[0]); i++)
    {
        MPlugin plug;
        char    msg[64];
        make_plugin(&plug);

        snprintf(msg, sizeof(msg), "ini_parseline - comment line #%u", i);
        TEST(msg);
        ASSERT_FALSE(plug.ini_parseline(lines[i]));
        ASSERT_INT(meta_errno, ME_COMMENT);
        PASS();
    }
    return 0;
}

static int test_ini_parseline_os_not_supported(void)
{
    MPlugin plug;
    make_plugin(&plug);

    TEST("ini_parseline - entry for another platform reports ME_OSNOTSUP");
    ASSERT_FALSE(plug.ini_parseline("lin128 dlls/mm_test.so Test Plugin"));
    ASSERT_INT(meta_errno, ME_OSNOTSUP);
    PASS();
    return 0;
}

static int test_ini_parseline_format_error(void)
{
    MPlugin plug;
    char    line[64];
    make_plugin(&plug);

    TEST("ini_parseline - platform without filename reports ME_FORMAT");
    snprintf(line, sizeof(line), "%s   ", PLATFORM_SPC);
    ASSERT_FALSE(plug.ini_parseline(line));
    ASSERT_INT(meta_errno, ME_FORMAT);
    PASS();
    return 0;
}

static int test_ini_parseline_unknown_platform(void)
{
    MPlugin plug;
    make_plugin(&plug);

    TEST("ini_parseline - unknown platform token reports ME_OSNOTSUP");
    ASSERT_FALSE(plug.ini_parseline("not-a-platform dlls/mm_test.so Test Plugin"));
    ASSERT_INT(meta_errno, ME_OSNOTSUP);
    PASS();
    return 0;
}

static int test_ini_parseline_platform_aliases_and_arch_isolation(void)
{
    MPlugin plug;
    char    line[256];

    TEST("ini_parseline - generic PLATFORM accepted with pfspecific=0");
    make_plugin(&plug);
    snprintf(line, sizeof(line), "%s addons/metamod/dlls/test%s", PLATFORM, PLATFORM_DLEXT);
    ASSERT_TRUE(plug.ini_parseline(line));
    ASSERT_INT(plug.pfspecific, 0);
    PASS();

#ifdef PLATFORM_SPC_ALIAS
    TEST("ini_parseline - PLATFORM_SPC_ALIAS accepted with pfspecific=1");
    make_plugin(&plug);
    snprintf(line, sizeof(line), "%s addons/metamod/dlls/%s/test%s", PLATFORM_SPC_ALIAS, PLATFORM_DLL_SUBDIR, PLATFORM_DLEXT);
    ASSERT_TRUE(plug.ini_parseline(line));
    ASSERT_INT(plug.pfspecific, 1);
    PASS();
#endif

    static const char* const kOppositeTokens[] = {
#if defined(_WIN64)
        "win32",
        "linux",
        "lin32",
        "lin64",
        "linux32",
        "linux64",
#elif defined(_WIN32)
        "win64",
        "linux",
        "lin32",
        "lin64",
        "linux32",
        "linux64",
#elif defined(__x86_64__) || defined(__amd64__)
        "lin32",
        "linux32",
        "win32",
        "win64",
        "mswin",
#else
        "lin64",
        "linux64",
        "win32",
        "win64",
        "mswin",
#endif
    };

    for (unsigned int i = 0; i < sizeof(kOppositeTokens) / sizeof(kOppositeTokens[0]); i++)
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "ini_parseline - rejects mismatched token '%s'", kOppositeTokens[i]);
        TEST(msg);
        make_plugin(&plug);
        snprintf(line, sizeof(line), "%s addons/metamod/dlls/test%s", kOppositeTokens[i], PLATFORM_DLEXT);
        ASSERT_FALSE(plug.ini_parseline(line));
        ASSERT_INT(meta_errno, ME_OSNOTSUP);
        PASS();
    }

    return 0;
}

static int test_resolve_arch_subdir_fallback(void)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path        tempRoot = fs::temp_directory_path(ec) / ("mmfg_test_resolve_" PLATFORM_DLL_SUBDIR);
    fs::remove_all(tempRoot, ec);

    fs::path archDir    = tempRoot / "addons" / "metamod" / "dlls" / PLATFORM_DLL_SUBDIR;
    fs::path pluginFile = archDir / (std::string("asext") + PLATFORM_DLEXT);
    fs::create_directories(archDir, ec);
    {
        std::ofstream out(pluginFile, std::ios::binary);
        out << "stub";
    }

    std::string gamedir = tempRoot.generic_string();
    mm_test_set_gamedir(gamedir.c_str());
    STRNCPY(GameDLL.gamedir, gamedir.c_str(), sizeof(GameDLL.gamedir));

    std::string legacyRel   = std::string("addons/metamod/dlls/asext") + PLATFORM_DLEXT;
    std::string expectedRel = std::string("addons/metamod/dlls/") + PLATFORM_DLL_SUBDIR + "/asext" + PLATFORM_DLEXT;
    std::string expectedAbs = gamedir + "/" + expectedRel;
#ifdef _WIN32
    char expectedAbsNorm[PATH_MAX];
    STRNCPY(expectedAbsNorm, expectedAbs.c_str(), sizeof(expectedAbsNorm));
    normalize_pathname(expectedAbsNorm);
    expectedAbs = expectedAbsNorm;
    char gamedirNorm[PATH_MAX];
    STRNCPY(gamedirNorm, gamedir.c_str(), sizeof(gamedirNorm));
    normalize_pathname(gamedirNorm);
    STRNCPY(GameDLL.gamedir, gamedirNorm, sizeof(GameDLL.gamedir));
#endif

    MPlugin plug;
    TEST("resolve - legacy flat dlls path falls back to arch subdir");
    make_plugin(&plug);
    ASSERT_TRUE(plug.plugin_parseline(legacyRel.c_str(), 0));
    ASSERT_TRUE(plug.resolve());
    ASSERT_STR(plug.pathname, expectedAbs.c_str());
    ASSERT_STR(plug.filename, expectedRel.c_str());
    PASS();

    TEST("resolve - explicit wrong subdir is not redirected");
    make_plugin(&plug);
    std::string wrongSubdirRel = std::string("addons/metamod/dlls/other/asext") + PLATFORM_DLEXT;
    ASSERT_TRUE(plug.plugin_parseline(wrongSubdirRel.c_str(), 0));
    ASSERT_FALSE(plug.resolve());
    ASSERT_INT(meta_errno, ME_NOTFOUND);
    PASS();

    fs::remove_all(tempRoot, ec);
    return 0;
}

int main(void)
{
    int fail = 0;

    mm_test_reset();

    fail |= test_ini_parseline_empty_line();
    fail |= test_ini_parseline_whitespace_only();
    fail |= test_ini_parseline_trims_whitespace();
    fail |= test_ini_parseline_trailing_whitespace_only();
    fail |= test_ini_parseline_file_points_into_plugin();
    fail |= test_ini_parseline_comments();
    fail |= test_ini_parseline_os_not_supported();
    fail |= test_ini_parseline_format_error();
    fail |= test_ini_parseline_unknown_platform();
    fail |= test_ini_parseline_platform_aliases_and_arch_isolation();
    fail |= test_resolve_arch_subdir_fallback();

    printf("\n%d/%d checks passed\n", tests_passed, tests_run);
    return fail;
}
