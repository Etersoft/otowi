/*
 * NT threads support
 *
 * Copyright 1996, 2003 Alexandre Julliard
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

#include <assert.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/types.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif
#ifdef HAVE_SYS_SYSCALL_H
#include <sys/syscall.h>
#endif

#define NONAMELESSUNION
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"
#include "wine/library.h"
#include "wine/server.h"
#include "wine/debug.h"
#include "ntdll_misc.h"
#include "ddk/wdm.h"
#include "wine/exception.h"

WINE_DEFAULT_DEBUG_CHANNEL(thread);

#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN 16384
#endif

struct _KUSER_SHARED_DATA *user_shared_data = NULL;
static const WCHAR default_windirW[] = {'C',':','\\','w','i','n','d','o','w','s',0};

//PUNHANDLED_EXCEPTION_FILTER unhandled_exception_filter = NULL;
//void (WINAPI *kernel32_start_process)(LPTHREAD_START_ROUTINE,void*) = NULL;

/* info passed to a starting thread */
struct startup_info
{
    TEB                            *teb;
    PRTL_THREAD_START_ROUTINE       entry_point;
    void                           *entry_arg;
};

static PEB *peb;
static PEB_LDR_DATA ldr;
static RTL_USER_PROCESS_PARAMETERS params;  /* default parameters if no parent */
static WCHAR current_dir[MAX_NT_PATH_LENGTH];
static RTL_BITMAP tls_bitmap;
static RTL_BITMAP tls_expansion_bitmap;
static RTL_BITMAP fls_bitmap;
static int nb_threads = 1;

static RTL_CRITICAL_SECTION peb_lock;
static RTL_CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &peb_lock,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": peb_lock") }
};
static RTL_CRITICAL_SECTION peb_lock = { &critsect_debug, -1, 0, 0, 0, 0 };

/***********************************************************************
 *           get_unicode_string
 *
 * Copy a unicode string from the startup info.
 */
static inline void get_unicode_string( UNICODE_STRING *str, WCHAR **src, WCHAR **dst, UINT len )
{
    str->Buffer = *dst;
    str->Length = len;
    str->MaximumLength = len + sizeof(WCHAR);
    memcpy( str->Buffer, *src, len );
    str->Buffer[len / sizeof(WCHAR)] = 0;
    *src += len / sizeof(WCHAR);
    *dst += len / sizeof(WCHAR) + 1;
}


/***********************************************************************
 *           thread_init
 *
 * Setup the initial thread.
 *
 * NOTES: The first allocated TEB on NT is at 0x7ffde000.
 */
HANDLE thread_init(void)
{
    TEB *teb;
    void *addr;
    BOOL suspend;
    SIZE_T size, info_size;
    HANDLE exe_file = 0;
    LARGE_INTEGER now;
    NTSTATUS status;
    struct ntdll_thread_data *thread_data;
    static struct debug_info debug_info;  /* debug info for initial thread */

#if 0
//    virtual_init();

    /* reserve space for shared user data */

    addr = (void *)0x7ffe0000;
    size = 0x10000;
    status = NtAllocateVirtualMemory( NtCurrentProcess(), &addr, 0, &size,
                                      MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );
    if (status)
    {
        MESSAGE( "wine: failed to map the shared user data: %08x\n", status );
        exit(1);
    }
    user_shared_data = addr;
    memcpy( user_shared_data->NtSystemRoot, default_windirW, sizeof(default_windirW) );

    /* allocate and initialize the PEB */

    addr = NULL;
    size = sizeof(*peb);
    NtAllocateVirtualMemory( NtCurrentProcess(), &addr, 1, &size,
                             MEM_COMMIT | MEM_TOP_DOWN, PAGE_READWRITE );
    peb = addr;
#endif
    // FIXME
    peb = malloc(sizeof(*peb));

    peb->FastPebLock        = &peb_lock;
    peb->ProcessParameters  = &params;
    peb->TlsBitmap          = &tls_bitmap;
    peb->TlsExpansionBitmap = &tls_expansion_bitmap;
    peb->FlsBitmap          = &fls_bitmap;
    peb->LdrData            = &ldr;
    peb->OSMajorVersion     = 5;
    peb->OSMinorVersion     = 1;
    peb->OSBuildNumber      = 0xA28;
    peb->OSPlatformId       = VER_PLATFORM_WIN32_NT;
    params.CurrentDirectory.DosPath.Buffer = current_dir;
    params.CurrentDirectory.DosPath.MaximumLength = sizeof(current_dir);
    params.wShowWindow = 1; /* SW_SHOWNORMAL */
    ldr.Length = sizeof(ldr);
    ldr.Initialized = TRUE;
#if 0
    RtlInitializeBitMap( &tls_bitmap, peb->TlsBitmapBits, sizeof(peb->TlsBitmapBits) * 8 );
    RtlInitializeBitMap( &tls_expansion_bitmap, peb->TlsExpansionBitmapBits,
                         sizeof(peb->TlsExpansionBitmapBits) * 8 );
    RtlInitializeBitMap( &fls_bitmap, peb->FlsBitmapBits, sizeof(peb->FlsBitmapBits) * 8 );
    RtlSetBits( peb->TlsBitmap, 0, 1 ); /* TLS index 0 is reserved and should be initialized to NULL. */
    RtlSetBits( peb->FlsBitmap, 0, 1 );
    InitializeListHead( &peb->FlsListHead );
    InitializeListHead( &ldr.InLoadOrderModuleList );
    InitializeListHead( &ldr.InMemoryOrderModuleList );
    InitializeListHead( &ldr.InInitializationOrderModuleList );
    *(ULONG_PTR *)peb->Reserved = get_image_addr();
#endif

    /*
     * Starting with Vista, the first user to log on has session id 1.
     * Session id 0 is for processes that don't interact with the user (like services).
     */
    peb->SessionId = 1;

    /* allocate and initialize the initial TEB */

//    signal_alloc_thread( &teb );

    // TODO: improve signal code
    teb = NtCurrentTeb();
    teb->Peb = peb;
    teb->Tib.StackBase = (void *)~0UL;
    teb->StaticUnicodeString.Buffer = teb->StaticUnicodeBuffer;
    teb->StaticUnicodeString.MaximumLength = sizeof(teb->StaticUnicodeBuffer);

    thread_data = (struct ntdll_thread_data *)&teb->GdiTebBatch;
    thread_data->request_fd = -1;
    thread_data->reply_fd   = -1;
    thread_data->wait_fd[0] = -1;
    thread_data->wait_fd[1] = -1;
    thread_data->debug_info = &debug_info;

#if 0
    signal_init_thread( teb );
    virtual_init_threading();
#endif

    debug_info.str_pos = debug_info.strings;
    debug_info.out_pos = debug_info.output;
    debug_init();

#if 0
    /* setup the server connection */
    server_init_process();
    info_size = server_init_thread( peb, &suspend );

    /* create the process heap */
    if (!(peb->ProcessHeap = RtlCreateHeap( HEAP_GROWABLE, NULL, 0, 0, NULL, NULL )))
    {
        MESSAGE( "wine: failed to create the process heap\n" );
        exit(1);
    }

    /* allocate user parameters */
    if (info_size)
    {
        init_user_process_params( info_size, &exe_file );
    }
    else
    {
        if (isatty(0) || isatty(1) || isatty(2))
            params.ConsoleHandle = (HANDLE)2; /* see kernel32/kernel_private.h */
        if (!isatty(0))
            wine_server_fd_to_handle( 0, GENERIC_READ|SYNCHRONIZE,  OBJ_INHERIT, &params.hStdInput );
        if (!isatty(1))
            wine_server_fd_to_handle( 1, GENERIC_WRITE|SYNCHRONIZE, OBJ_INHERIT, &params.hStdOutput );
        if (!isatty(2))
            wine_server_fd_to_handle( 2, GENERIC_WRITE|SYNCHRONIZE, OBJ_INHERIT, &params.hStdError );
    }

    /* initialize time values in user_shared_data */
    NtQuerySystemTime( &now );
    user_shared_data->SystemTime.LowPart = now.u.LowPart;
    user_shared_data->SystemTime.High1Time = user_shared_data->SystemTime.High2Time = now.u.HighPart;
    user_shared_data->u.TickCountQuad = (now.QuadPart - server_start_time) / 10000;
    user_shared_data->u.TickCount.High2Time = user_shared_data->u.TickCount.High1Time;
    user_shared_data->TickCountLowDeprecated = user_shared_data->u.TickCount.LowPart;
    user_shared_data->TickCountMultiplier = 1 << 24;

    fill_cpu_info();

    NtCreateKeyedEvent( &keyed_event, GENERIC_READ | GENERIC_WRITE, NULL, 0 );
#endif
    return exe_file;
}


/******************************************************************************
 *              NtSetInformationThread  (NTDLL.@)
 *              ZwSetInformationThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetInformationThread( HANDLE handle, THREADINFOCLASS class,
                                        LPCVOID data, ULONG length )
{
    NTSTATUS status;
    FIXME( "info class %d not supported yet\n", class );
    return STATUS_SUCCESS;

}
