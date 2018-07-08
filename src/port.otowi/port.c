
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "winerror.h"
#include "winbase.h"

#include "kernel_private.h"
#include "ntdll_misc.h"

#include "wine/exception.h"
#include "wine/debug.h"

#include "port.h"

WINE_DEFAULT_DEBUG_CHANNEL(otowi);

// ntdll/loader.c
static HANDLE main_exe_file;

// ntdll/server.c
timeout_t server_start_time = 0;  /* time of server startup */

// TODO: do as __wine_kernel_init
void otowi_init()
{
	// TODO: do as __wine_process_init
	main_exe_file = thread_init();
	LOCALE_Init();
}

// from dlls/winecrt0/exception.c
DWORD __wine_exception_handler_page_fault( EXCEPTION_RECORD *record,
                                           EXCEPTION_REGISTRATION_RECORD *frame,
                                           CONTEXT *context,
                                           EXCEPTION_REGISTRATION_RECORD **pdispatcher )
{
    if (record->ExceptionFlags & (EH_UNWINDING | EH_EXIT_UNWIND | EH_NESTED_CALL))
        return ExceptionContinueSearch;
    if (record->ExceptionCode != STATUS_ACCESS_VIOLATION)
        return ExceptionContinueSearch;

    //unwind_frame( record, frame );
}

/*******************************************************************
 *		raise_exception
 *
 * Implementation of NtRaiseException.
 */
static NTSTATUS raise_exception( EXCEPTION_RECORD *rec, CONTEXT *context, BOOL first_chance )
{
    FIXME("raise_exception");
    exit(1);
}

/*******************************************************************
 *		NtRaiseException (NTDLL.@)
 */
NTSTATUS WINAPI NtRaiseException( EXCEPTION_RECORD *rec, CONTEXT *context, BOOL first_chance )
{
    NTSTATUS status = raise_exception( rec, context, first_chance );
    //if (status == STATUS_SUCCESS) NtSetContextThread( GetCurrentThread(), context );
    return status;
}


/***********************************************************************
 *		RtlRaiseException (NTDLL.@)
 */
void WINAPI RtlRaiseException( EXCEPTION_RECORD *rec )
{
    CONTEXT context;
    NTSTATUS status;

    //RtlCaptureContext( &context );
    //rec->ExceptionAddress = (LPVOID)context.Pc;
    status = raise_exception( rec, &context, TRUE );
    //if (status) raise_status( status, rec );
}

