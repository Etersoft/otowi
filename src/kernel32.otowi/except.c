/*
 * Win32 exception functions
 *
 * Copyright (c) 1996 Onno Hovers, (onno@stack.urc.tue.nl)
 * Copyright (c) 1999 Alexandre Julliard
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
 *
 * Notes:
 *  What really happens behind the scenes of those new
 *  __try{...}__except(..){....}  and
 *  __try{...}__finally{...}
 *  statements is simply not documented by Microsoft. There could be different
 *  reasons for this:
 *  One reason could be that they try to hide the fact that exception
 *  handling in Win32 looks almost the same as in OS/2 2.x.
 *  Another reason could be that Microsoft does not want others to write
 *  binary compatible implementations of the Win32 API (like us).
 *
 *  Whatever the reason, THIS SUCKS!! Ensuring portability or future
 *  compatibility may be valid reasons to keep some things undocumented.
 *  But exception handling is so basic to Win32 that it should be
 *  documented!
 *
 */
#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#include <stdio.h>
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wingdi.h"
#include "winuser.h"
#include "wine/exception.h"
#include "wine/library.h"
#include "excpt.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(seh);

static PTOP_LEVEL_EXCEPTION_FILTER top_filter;

/*******************************************************************
 *         RaiseException  (KERNEL32.@)
 */
void WINAPI RaiseException( DWORD code, DWORD flags, DWORD nbargs, const ULONG_PTR *args )
{
    EXCEPTION_RECORD record;

    /* Compose an exception record */

    record.ExceptionCode    = code;
    record.ExceptionFlags   = flags & EH_NONCONTINUABLE;
    record.ExceptionRecord  = NULL;
    record.ExceptionAddress = RaiseException;
    if (nbargs && args)
    {
        if (nbargs > EXCEPTION_MAXIMUM_PARAMETERS) nbargs = EXCEPTION_MAXIMUM_PARAMETERS;
        record.NumberParameters = nbargs;
        memcpy( record.ExceptionInformation, args, nbargs * sizeof(*args) );
    }
    else record.NumberParameters = 0;

    RtlRaiseException( &record );
}


/*******************************************************************
 *         format_exception_msg
 */
static int format_exception_msg( const EXCEPTION_POINTERS *ptr, char *buffer, int size )
{
    const EXCEPTION_RECORD *rec = ptr->ExceptionRecord;
    int len,len2;

    switch(rec->ExceptionCode)
    {
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        len = snprintf( buffer, size, "Unhandled division by zero" );
        break;
    case EXCEPTION_INT_OVERFLOW:
        len = snprintf( buffer, size, "Unhandled overflow" );
        break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        len = snprintf( buffer, size, "Unhandled array bounds" );
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        len = snprintf( buffer, size, "Unhandled illegal instruction" );
        break;
    case EXCEPTION_STACK_OVERFLOW:
        len = snprintf( buffer, size, "Unhandled stack overflow" );
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        len = snprintf( buffer, size, "Unhandled privileged instruction" );
        break;
    case EXCEPTION_ACCESS_VIOLATION:
        if (rec->NumberParameters == 2)
            len = snprintf( buffer, size, "Unhandled page fault on %s access to 0x%08lx",
                            rec->ExceptionInformation[0] == EXCEPTION_WRITE_FAULT ? "write" :
                            rec->ExceptionInformation[0] == EXCEPTION_EXECUTE_FAULT ? "execute" : "read",
                            rec->ExceptionInformation[1]);
        else
            len = snprintf( buffer, size, "Unhandled page fault");
        break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        len = snprintf( buffer, size, "Unhandled alignment" );
        break;
    case CONTROL_C_EXIT:
        len = snprintf( buffer, size, "Unhandled ^C");
        break;
    case STATUS_POSSIBLE_DEADLOCK:
        len = snprintf( buffer, size, "Critical section %08lx wait failed",
                 rec->ExceptionInformation[0]);
        break;
    case EXCEPTION_WINE_STUB:
        if ((ULONG_PTR)rec->ExceptionInformation[1] >> 16)
            len = snprintf( buffer, size, "Unimplemented function %s.%s called",
                            (char *)rec->ExceptionInformation[0], (char *)rec->ExceptionInformation[1] );
        else
            len = snprintf( buffer, size, "Unimplemented function %s.%ld called",
                            (char *)rec->ExceptionInformation[0], rec->ExceptionInformation[1] );
        break;
    case EXCEPTION_WINE_ASSERTION:
        len = snprintf( buffer, size, "Assertion failed" );
        break;
    default:
        len = snprintf( buffer, size, "Unhandled exception 0x%08x in thread %x", rec->ExceptionCode, GetCurrentThreadId());
        break;
    }
    if ((len<0) || (len>=size))
        return -1;
#ifdef __i386__
    if (LOWORD(ptr->ContextRecord->SegCs) != wine_get_cs())
        len2 = snprintf(buffer+len, size-len, " at address 0x%04x:0x%08x",
                        LOWORD(ptr->ContextRecord->SegCs),
                        (DWORD)ptr->ExceptionRecord->ExceptionAddress);
    else
#endif
        len2 = snprintf(buffer+len, size-len, " at address %p",
                        ptr->ExceptionRecord->ExceptionAddress);
    if ((len2<0) || (len>=size-len))
        return -1;
    return len+len2;
}


#if 0
/*******************************************************************
 *         check_resource_write
 *
 * Check if the exception is a write attempt to the resource data.
 * If yes, we unprotect the resources to let broken apps continue
 * (Windows does this too).
 */
static inline BOOL check_resource_write( void *addr )
{
    DWORD old_prot;
    void *rsrc;
    DWORD size;
    MEMORY_BASIC_INFORMATION info;

    if (!VirtualQuery( addr, &info, sizeof(info) )) return FALSE;
    if (info.State == MEM_FREE || !(info.Type & MEM_IMAGE)) return FALSE;
    if (!(rsrc = RtlImageDirectoryEntryToData( info.AllocationBase, TRUE,
                                              IMAGE_DIRECTORY_ENTRY_RESOURCE, &size )))
        return FALSE;
    if (addr < rsrc || (char *)addr >= (char *)rsrc + size) return FALSE;
    TRACE( "Broken app is writing to the resource data, enabling work-around\n" );
    VirtualProtect( rsrc, size, PAGE_READWRITE, &old_prot );
    return TRUE;
}
#endif

/*******************************************************************
 *         UnhandledExceptionFilter   (KERNEL32.@)
 */
LONG WINAPI UnhandledExceptionFilter(PEXCEPTION_POINTERS epointers)
{
    const EXCEPTION_RECORD *rec = epointers->ExceptionRecord;

    if (rec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2)
    {
        switch(rec->ExceptionInformation[0])
        {
        case EXCEPTION_WRITE_FAULT:
            //if (check_resource_write( (void *)rec->ExceptionInformation[1] ))
            //    return EXCEPTION_CONTINUE_EXECUTION;
            break;
        }
    }

    if (!NtCurrentTeb()->Peb->BeingDebugged)
    {
        if (rec->ExceptionCode == CONTROL_C_EXIT)
        {
            /* do not launch the debugger on ^C, simply terminate the process */
            TerminateProcess( GetCurrentProcess(), 1 );
        }

        if (top_filter)
        {
            LONG ret = top_filter( epointers );
            if (ret != EXCEPTION_CONTINUE_SEARCH) return ret;
        }

        /* FIXME: Should check the current error mode */

        //if (!start_debugger_atomic( epointers ) || !NtCurrentTeb()->Peb->BeingDebugged)
        //    return EXCEPTION_EXECUTE_HANDLER;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}


/***********************************************************************
 *            SetUnhandledExceptionFilter   (KERNEL32.@)
 */
LPTOP_LEVEL_EXCEPTION_FILTER WINAPI DECLSPEC_HOTPATCH SetUnhandledExceptionFilter(
                                          LPTOP_LEVEL_EXCEPTION_FILTER filter )
{
    LPTOP_LEVEL_EXCEPTION_FILTER old = top_filter;
    top_filter = filter;
    return old;
}


/**************************************************************************
 *           FatalAppExitA   (KERNEL32.@)
 */
void WINAPI FatalAppExitA( UINT action, LPCSTR str )
{
    WARN("AppExit\n");

    ERR( "%s\n", debugstr_a(str) );
    ExitProcess(0);
}


/**************************************************************************
 *           FatalAppExitW   (KERNEL32.@)
 */
void WINAPI FatalAppExitW( UINT action, LPCWSTR str )
{
    WARN("AppExit\n");

    ERR( "%s\n", debugstr_w(str) );
    ExitProcess(0);
}


/**************************************************************************
 *           FatalExit   (KERNEL32.@)
 */
void WINAPI FatalExit(int ExitCode)
{
    WARN("FatalExit\n");
    ExitProcess(ExitCode);
}
