/*
 * Process environment management
 *
 * Copyright 1996, 1998 Alexandre Julliard
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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "wine/library.h"
#include "winternl.h"
#include "wine/unicode.h"
#include "wine/debug.h"

#include "kernel_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(environ);


/***********************************************************************
 *           GetStdHandle    (KERNEL32.@)
 */
HANDLE WINAPI GetStdHandle( DWORD std_handle )
{
    switch (std_handle)
    {
        case STD_INPUT_HANDLE:  return NtCurrentTeb()->Peb->ProcessParameters->hStdInput;
        case STD_OUTPUT_HANDLE: return NtCurrentTeb()->Peb->ProcessParameters->hStdOutput;
        case STD_ERROR_HANDLE:  return NtCurrentTeb()->Peb->ProcessParameters->hStdError;
    }
    SetLastError( ERROR_INVALID_HANDLE );
    return INVALID_HANDLE_VALUE;
}


/***********************************************************************
 *           SetStdHandle    (KERNEL32.@)
 */
BOOL WINAPI SetStdHandle( DWORD std_handle, HANDLE handle )
{
    switch (std_handle)
    {
        case STD_INPUT_HANDLE:  NtCurrentTeb()->Peb->ProcessParameters->hStdInput = handle;  return TRUE;
        case STD_OUTPUT_HANDLE: NtCurrentTeb()->Peb->ProcessParameters->hStdOutput = handle; return TRUE;
        case STD_ERROR_HANDLE:  NtCurrentTeb()->Peb->ProcessParameters->hStdError = handle;  return TRUE;
    }
    SetLastError( ERROR_INVALID_HANDLE );
    return FALSE;
}


/***********************************************************************
 *           GetEnvironmentStringsA   (KERNEL32.@)
 *           GetEnvironmentStrings    (KERNEL32.@)
 */
LPSTR WINAPI GetEnvironmentStringsA(void)
{
    LPWSTR      ptrW;
    unsigned    len, slen;
    LPSTR       ret, ptrA;

    RtlAcquirePebLock();

    len = 1;

    ptrW = NtCurrentTeb()->Peb->ProcessParameters->Environment;
    while (*ptrW)
    {
        slen = strlenW(ptrW) + 1;
        len += WideCharToMultiByte( CP_ACP, 0, ptrW, slen, NULL, 0, NULL, NULL );
        ptrW += slen;
    }

    if ((ret = HeapAlloc( GetProcessHeap(), 0, len )) != NULL)
    {
        ptrW = NtCurrentTeb()->Peb->ProcessParameters->Environment;
        ptrA = ret;
        while (*ptrW)
        {
            slen = strlenW(ptrW) + 1;
            WideCharToMultiByte( CP_ACP, 0, ptrW, slen, ptrA, len, NULL, NULL );
            ptrW += slen;
            ptrA += strlen(ptrA) + 1;
        }
        *ptrA = 0;
    }

    RtlReleasePebLock();
    return ret;
}


/***********************************************************************
 *           GetEnvironmentStringsW   (KERNEL32.@)
 */
LPWSTR WINAPI GetEnvironmentStringsW(void)
{
    return NtCurrentTeb()->Peb->ProcessParameters->Environment;
}


/***********************************************************************
 *           FreeEnvironmentStringsA   (KERNEL32.@)
 */
BOOL WINAPI FreeEnvironmentStringsA( LPSTR ptr )
{
    return HeapFree( GetProcessHeap(), 0, ptr );
}


/***********************************************************************
 *           FreeEnvironmentStringsW   (KERNEL32.@)
 */
BOOL WINAPI FreeEnvironmentStringsW( LPWSTR ptr )
{
    return TRUE;
}


/***********************************************************************
 *           GetEnvironmentVariableA   (KERNEL32.@)
 */
DWORD WINAPI GetEnvironmentVariableA( LPCSTR name, LPSTR value, DWORD size )
{
    UNICODE_STRING      us_name;
    PWSTR               valueW;
    DWORD               ret;

    if (!name || !*name)
    {
        SetLastError(ERROR_ENVVAR_NOT_FOUND);
        return 0;
    }

    /* limit the size to sane values */
    size = min(size, 32767);
    if (!(valueW = HeapAlloc(GetProcessHeap(), 0, size * sizeof(WCHAR))))
        return 0;

    RtlCreateUnicodeStringFromAsciiz( &us_name, name );
    SetLastError(0);
    ret = GetEnvironmentVariableW( us_name.Buffer, valueW, size);
    if (ret && ret < size)
    {
        WideCharToMultiByte( CP_ACP, 0, valueW, ret + 1, value, size, NULL, NULL );
    }
    /* this is needed to tell, with 0 as a return value, the difference between:
     * - an error (GetLastError() != 0)
     * - returning an empty string (in this case, we need to update the buffer)
     */
    if (ret == 0 && size && GetLastError() == 0)
        value[0] = '\0';

    RtlFreeUnicodeString( &us_name );
    HeapFree(GetProcessHeap(), 0, valueW);

    return ret;
}


/***********************************************************************
 *           GetEnvironmentVariableW   (KERNEL32.@)
 */
DWORD WINAPI GetEnvironmentVariableW( LPCWSTR name, LPWSTR val, DWORD size )
{
    UNICODE_STRING      us_name;
    UNICODE_STRING      us_value;
    NTSTATUS            status;
    unsigned            len;

    TRACE("(%s %p %u)\n", debugstr_w(name), val, size);

    if (!name || !*name)
    {
        SetLastError(ERROR_ENVVAR_NOT_FOUND);
        return 0;
    }

    RtlInitUnicodeString(&us_name, name);
    us_value.Length = 0;
    us_value.MaximumLength = (size ? size - 1 : 0) * sizeof(WCHAR);
    us_value.Buffer = val;

    status = RtlQueryEnvironmentVariable_U(NULL, &us_name, &us_value);
    len = us_value.Length / sizeof(WCHAR);
    if (status != STATUS_SUCCESS)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        return (status == STATUS_BUFFER_TOO_SMALL) ? len + 1 : 0;
    }
    if (size) val[len] = '\0';

    return us_value.Length / sizeof(WCHAR);
}


