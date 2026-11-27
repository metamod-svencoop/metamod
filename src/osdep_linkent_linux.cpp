/*
 * Copyright (c) 2004-2006 Jussi Kivilinna
 *
 *    This file is part of "Metamod All-Mod-Support"-patch for Metamod.
 *
 *    Metamod is free software; you can redistribute it and/or modify it
 *    under the terms of the GNU General Public License as published by the
 *    Free Software Foundation; either version 2 of the License, or (at
 *    your option) any later version.
 *
 *    Metamod is distributed in the hope that it will be useful, but
 *    WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Metamod; if not, write to the Free Software Foundation,
 *    Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *    In addition, as a special exception, the author gives permission to
 *    link the code of this program with the Half-Life Game Engine ("HL
 *    Engine") and Modified Game Libraries ("MODs") developed by Valve,
 *    L.L.C ("Valve").  You must obey the GNU General Public License in all
 *    respects for all of the code used other than the HL Engine and MODs
 *    from Valve.  If you modify this file, you may extend this exception
 *    to your version of the file, but you are not obligated to do so.  If
 *    you do not wish to do so, delete this exception statement from your
 *    version.
 *
 */

#include <extdll.h> // always

#ifndef __USE_GNU
#    define __USE_GNU
#endif

#include <dlfcn.h>
#include <link.h>

#include "osdep.h"
#include "osdep_p.h"
#include "log_meta.h" // META_LOG, etc
#include "support_meta.h"

//Added by hzqst
#include "detours.h" // ALERT, etc
#include "mutil.h"   // ALERT, etc

//
// Linux code for dynamic linkents
//  -- by Jussi Kivilinna
//

typedef void* (*dlsym_func)(void* module, const char* funcname);

static void* gamedll_module_handle = 0;
static void* metamod_module_handle = 0;

//pointer to original dlsym
static dlsym_func dlsym_original;

//checks if pointer x points to jump forwarder
inline bool is_code_trampoline_jmp_opcode(void* x)
{
    return (((unsigned char*)x)[0] == 0xff && ((unsigned char*)x)[1] == 0x25);
}

//extracts pointer from "jmp dword ptr[pointer]"
inline void* extract_function_pointer_from_trampoline_jmp(void* x)
{
    void** ptr;
    memcpy(&ptr, (char*)x + 2, sizeof(ptr));
    return *ptr;
}

//
// Replacement dlsym function
//
static void* __replacement_dlsym(void* pmodule, const char* funcname)
{
    //Added by hzqst, as simple as possible
    auto r = dlsym_original(pmodule, funcname);
    if (!r && pmodule == metamod_module_handle && metamod_module_handle && gamedll_module_handle)
    {
        r = dlsym_original(gamedll_module_handle, funcname);
    }
    return r;
}

static hook_t* g_dlsym_hook = NULL;

//
// Initialize
//
int DLLINTERNAL init_linkent_replacement(DLHANDLE MetamodHandle, DLHANDLE GameDllHandle)
{
    metamod_module_handle = MetamodHandle;
    gamedll_module_handle = GameDllHandle;

    // dlsym is already known to be pointing to valid function, we loaded gamedll using it earlier!
    void* sym_ptr = (void*)&dlsym;
    while (is_code_trampoline_jmp_opcode(sym_ptr))
    {
        sym_ptr = extract_function_pointer_from_trampoline_jmp(sym_ptr);
    }

    dlsym_original = (dlsym_func)sym_ptr;

    //Added by hzqst, use mutil
    g_dlsym_hook = MetaUtilFunctions.pfnInlineHook(sym_ptr, (void*)__replacement_dlsym, (void**)&dlsym_original, false);

    //done
    return (1);
}

//2022/07/08 Added by hzqst free newFunctions shits
void DLLINTERNAL uninit_linkent_replacement()
{
    if (g_dlsym_hook)
    {
        MetaUtilFunctions.pfnUnHook(g_dlsym_hook);
        g_dlsym_hook = NULL;
    }
}
