// Regression tests for Metamod 5:18 interface version compatibility:
// standard Metamod <= 5:13 plugins remain accepted, deprecated 5:14..5:17
// custom extension layouts are rejected, 5:18 is accepted, and newer/mismatched
// versions are rejected.
#include <cstdio>
#include <cstring>

#include <extdll.h>
#include <meta_api.h>

#include "metamod.h"
#include "mm_test_stubs.h"
#include "test_common.h"

typedef void (*SetFixtureInterfaceVersionFn)(const char*);

static int check_version(SetFixtureInterfaceVersionFn set_version, const char* version, bool expect_accept)
{
    mm_test_reset();

    DLL_FUNCTIONS           dll    = {};
    NEW_DLL_FUNCTIONS       newer  = {};
    sv_blending_interface_t studio = {};
    GameDLL.funcs.dllapi_table     = &dll;
    GameDLL.funcs.newapi_table     = &newer;
    GameDLL.funcs.studio_blend_api = &studio;
    GameDLL.file                   = const_cast<char*>("test-game");

    Plugins  = new MPluginList("plugins.ini");
    RegCmds  = new MRegCmdList();
    RegCvars = new MRegCvarList();

    MPlugin& plugin = Plugins->plist[0];
    plugin.status   = PL_VALID;
    plugin.action   = PA_LOAD;
    plugin.file     = plugin.filename;
    STRNCPY(plugin.filename, "fake_hook_plugin", sizeof(plugin.filename));
    STRNCPY(plugin.pathname, MMFG_TEST_HOOK_PLUGIN, sizeof(plugin.pathname));
    STRNCPY(plugin.desc, "fake_hook_plugin", sizeof(plugin.desc));

    set_version(version);
    meta_errno    = ME_NOERROR;
    mBOOL load_ok = plugin.load(PT_STARTUP);
    int   rc      = 0;

    if (expect_accept)
    {
        if (load_ok != mTRUE || meta_errno != ME_NOERROR || plugin.status != PL_RUNNING)
        {
            fprintf(stderr, "  FAIL: expected %s to load (ok=%d errno=%d status=%d)\n",
                    version, (int)load_ok, (int)meta_errno, (int)plugin.status);
            rc = 1;
        }
        if (plugin.status >= PL_RUNNING)
        {
            plugin.action = PA_UNLOAD;
            plugin.unload(PT_ANYTIME, PNL_COMMAND, PNL_COMMAND);
        }
    }
    else
    {
        if (load_ok == mTRUE || meta_errno != ME_IFVERSION)
        {
            fprintf(stderr, "  FAIL: expected %s to be rejected with ME_IFVERSION (ok=%d errno=%d status=%d)\n",
                    version, (int)load_ok, (int)meta_errno, (int)plugin.status);
            rc = 1;
        }
        if (plugin.status >= PL_RUNNING)
        {
            plugin.action = PA_UNLOAD;
            plugin.unload(PT_ANYTIME, PNL_COMMAND, PNL_COMMAND);
        }
        else
        {
            plugin.clear();
        }
    }

    delete Plugins;
    Plugins = NULL;
    delete RegCmds;
    RegCmds = NULL;
    delete RegCvars;
    RegCvars = NULL;

    return rc;
}

int main(void)
{
    int fail = 0;

    // Keep the fixture mapped across MPlugin::load() calls so its configured
    // interface version stays intact.
    DLHANDLE fixture = DLOPEN(MMFG_TEST_HOOK_PLUGIN);
    if (!fixture)
    {
        fprintf(stderr, "Failed to open fixture %s\n", MMFG_TEST_HOOK_PLUGIN);
        return 1;
    }

    auto set_version = reinterpret_cast<SetFixtureInterfaceVersionFn>(DLSYM(fixture, "SetFixtureInterfaceVersion"));
    if (!set_version)
    {
        fprintf(stderr, "Failed to resolve SetFixtureInterfaceVersion\n");
        DLCLOSE(fixture);
        return 1;
    }

    const char* accepted[] = {"5:12", "5:13", META_INTERFACE_VERSION};
    for (const char* version : accepted)
    {
        char label[96];
        snprintf(label, sizeof(label), "MPlugin::load accepts interface %s", version);
        TEST(label);
        if (check_version(set_version, version, true))
        {
            fail = 1;
        }
        else
        {
            PASS();
        }
    }

    const char* rejected[] = {"4:0", "5:14", "5:15", "5:16", "5:17", "5:19", "6:0"};
    for (const char* version : rejected)
    {
        char label[96];
        snprintf(label, sizeof(label), "MPlugin::load rejects interface %s", version);
        TEST(label);
        if (check_version(set_version, version, false))
        {
            fail = 1;
        }
        else
        {
            PASS();
        }
    }

    set_version(META_INTERFACE_VERSION);
    DLCLOSE(fixture);

    printf("\n%d/%d checks passed\n", tests_passed, tests_run);
    return fail;
}
