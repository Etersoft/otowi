/* from far2l */

#include <stdio.h>
#include <errno.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "winerror.h"
#include "winbase.h"

#include "wine/debug.h"

#include "port.h"

void init_directories(void)
{
}

/***********************************************************************
 *            RtlRaiseStatus  (NTDLL.@)
 *
 * Raise an exception with ExceptionCode = status
 */
void WINAPI RtlRaiseStatus( NTSTATUS status )
{
    //raise_status( status, NULL );
}

LPCSTR debugstr_us( const UNICODE_STRING *us )
{
    if (!us) return "<null>";
    return debugstr_wn(us->Buffer, us->Length / sizeof(WCHAR));
}

static TEB teb_buffer;

/**********************************************************************
 *           NtCurrentTeb   (NTDLL.@)
 */
TEB * WINAPI NtCurrentTeb(void)
{
    return &teb_buffer;
    //return pthread_getspecific( teb_key );
}
