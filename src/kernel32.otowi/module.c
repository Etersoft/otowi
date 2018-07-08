/*
 * Modules
 *
 * Copyright 1995 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include "wine/port.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winerror.h"
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "kernel_private.h"
#include "psapi.h"

#include "wine/exception.h"
#include "wine/list.h"
#include "wine/debug.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(module);


/***********************************************************************
 *              GetModuleHandleExA         (KERNEL32.@)
 */
BOOL WINAPI GetModuleHandleExA( DWORD flags, LPCSTR name, HMODULE *module )
{
    WCHAR *nameW;

    if (!name || (flags & GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS))
        return GetModuleHandleExW( flags, (LPCWSTR)name, module );

    if (!(nameW = FILE_name_AtoW( name, FALSE ))) return FALSE;
    return GetModuleHandleExW( flags, nameW, module );
}

/***********************************************************************
 *              GetModuleHandleExW         (KERNEL32.@)
 */
BOOL WINAPI GetModuleHandleExW( DWORD flags, LPCWSTR name, HMODULE *module )
{
    NTSTATUS status = STATUS_SUCCESS;
    HMODULE ret;
    ULONG_PTR magic;
    BOOL lock;

    if (!module)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    // TODO: any dll from Windows
    static const WCHAR ntdll[] = { 'n', 't', 'd', 'l', 'l','.','d','l','l', 0};
    if (!RtlCompareUnicodeStrings(name, strlenW(name), ntdll, strlenW(ntdll), TRUE)) {
        status = STATUS_SUCCESS;
        // TODO: всё равно возвращает 0 (по strace файл находит)
        *module = (PVOID)dlopen(0, RTLD_LAZY);
        //*module = (HMODULE)0xcece;
    } else {
        TRACE("try open %s\n", debugstr_w(name));
        status = STATUS_SUCCESS;
        *module = (HMODULE)0xcece;
    }

    return (status == STATUS_SUCCESS);
}

/***********************************************************************
 *              GetModuleHandleA         (KERNEL32.@)
 *
 * Get the handle of a dll loaded into the process address space.
 *
 * PARAMS
 *  module [I] Name of the dll
 *
 * RETURNS
 *  Success: A handle to the loaded dll.
 *  Failure: A NULL handle. Use GetLastError() to determine the cause.
 */
HMODULE WINAPI DECLSPEC_HOTPATCH GetModuleHandleA(LPCSTR module)
{
    HMODULE ret;

    GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, module, &ret );
    return ret;
}

/***********************************************************************
 *		GetModuleHandleW (KERNEL32.@)
 *
 * Unicode version of GetModuleHandleA.
 */
HMODULE WINAPI GetModuleHandleW(LPCWSTR module)
{
    HMODULE ret;

    GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, module, &ret );
    return ret;
}


/***********************************************************************
 *           GetProcAddress   		(KERNEL32.@)
 *
 * Find the address of an exported symbol in a loaded dll.
 *
 * PARAMS
 *  hModule  [I] Handle to the dll returned by LoadLibraryA().
 *  function [I] Name of the symbol, or an integer ordinal number < 16384
 *
 * RETURNS
 *  Success: A pointer to the symbol in the process address space.
 *  Failure: NULL. Use GetLastError() to determine the cause.
 */

FARPROC WINAPI GetProcAddress( HMODULE hModule, LPCSTR function )
{
	PVOID rv;
    if ( hModule <= (HMODULE)0xcece) {
        // TODO
        if (!strcmp(function, "RtlDosPathNameToNtPathName_U_WithStatus"))
            return &RtlDosPathNameToNtPathName_U_WithStatus;
        FIXME("skip GetProcAddress for fantom hModule %p", hModule);
        return NULL;
    }
	rv = (PVOID)dlsym((void *)hModule, function);
	if (!rv)
        ERR("GetProcAddress(%p, %s) - no such symbol\n", hModule, function);
	return rv;
}
