
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

#include "wine/unicode.h"
#include "wine/server.h"

#include "port.h"

WINE_DEFAULT_DEBUG_CHANNEL(otowi);

#define IS_SEPARATOR(ch)   ((ch) == '\\' || (ch) == '/')

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
	// TODO: do as kernel32/process_attach
	NtQuerySystemInformation( SystemBasicInformation, &system_info, sizeof(system_info), NULL );
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


// from ntdll/directory.c
/* return the length of the DOS namespace prefix if any */
static inline int get_dos_prefix_len( const UNICODE_STRING *name )
{
    static const WCHAR nt_prefixW[] = {'\\','?','?','\\'};
    static const WCHAR dosdev_prefixW[] = {'\\','D','o','s','D','e','v','i','c','e','s','\\'};

    if (name->Length >= sizeof(nt_prefixW) &&
        !memcmp( name->Buffer, nt_prefixW, sizeof(nt_prefixW) ))
        return sizeof(nt_prefixW) / sizeof(WCHAR);

    if (name->Length >= sizeof(dosdev_prefixW) &&
        !memicmpW( name->Buffer, dosdev_prefixW, sizeof(dosdev_prefixW)/sizeof(WCHAR) ))
        return sizeof(dosdev_prefixW) / sizeof(WCHAR);

    return 0;
}

// from ntdll/directory.c
/******************************************************************************
 *           wine_nt_to_unix_file_name  (NTDLL.@) Not a Windows API
 *
 * Convert a file name from NT namespace to Unix namespace.
 *
 * If disposition is not FILE_OPEN or FILE_OVERWRITE, the last path
 * element doesn't have to exist; in that case STATUS_NO_SUCH_FILE is
 * returned, but the unix name is still filled in properly.
 */
NTSTATUS CDECL wine_nt_to_unix_file_name( const UNICODE_STRING *nameW, ANSI_STRING *unix_name_ret,
                                          UINT disposition, BOOLEAN check_case )
{
    NTSTATUS status = STATUS_SUCCESS;
    const WCHAR *name, *p;
    //struct stat st;
    char *unix_name;
    int pos, ret, name_len, unix_len, prefix_len, used_default;

    name     = nameW->Buffer;
    name_len = nameW->Length / sizeof(WCHAR);

    if (!name_len || !IS_SEPARATOR(name[0])) return STATUS_OBJECT_PATH_SYNTAX_BAD;

    unix_len = ntdll_wcstoumbs( 0, name, name_len, NULL, 0, NULL, NULL );
    if (!(unix_name = RtlAllocateHeap( GetProcessHeap(), 0, unix_len )))
        return STATUS_NO_MEMORY;
    pos = strlen(unix_name);

    ret = ntdll_wcstoumbs( 0, name, name_len, unix_name, unix_len,
                           NULL, &used_default );
    if (!ret || used_default)
    {
        RtlFreeHeap( GetProcessHeap(), 0, unix_name );
        return STATUS_OBJECT_NAME_INVALID;
    }

//    if (status == STATUS_SUCCESS || status == STATUS_NO_SUCH_FILE)
    {
        TRACE( "%s -> %s\n", debugstr_us(nameW), debugstr_a(unix_name) );
        unix_name_ret->Buffer = unix_name;
        unix_name_ret->Length = strlen(unix_name);
        unix_name_ret->MaximumLength = unix_len;
    }
/*    else
    {
        TRACE( "%s not found in %s\n", debugstr_w(name), unix_name );
        RtlFreeHeap( GetProcessHeap(), 0, unix_name );
    }
*/
    return status;
}

int server_get_unix_fd( HANDLE handle, unsigned int wanted_access, int *unix_fd,
                        int *needs_close, enum server_fd_type *type, unsigned int *options )
{
    *unix_fd = (int)handle;
}
