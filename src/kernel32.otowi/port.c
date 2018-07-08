
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

#include "wine/exception.h"
#include "wine/debug.h"

#include "wine/unicode.h"
#include "wine/server.h"

#include "port.h"

WINE_DEFAULT_DEBUG_CHANNEL(otowi);

#define IS_SEPARATOR(ch)   ((ch) == '\\' || (ch) == '/')


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



void WINAPI ExitProcess( DWORD status )
{
    RtlExitUserProcess( status );
}

