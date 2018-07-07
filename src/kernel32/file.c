/*
 * File handling functions
 *
 * Copyright 1993 John Burton
 * Copyright 1996, 2004 Alexandre Julliard
 * Copyright 2008 Jeff Zaroyko
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
#include <stdio.h>
#include <errno.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "winerror.h"
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winioctl.h"
#include "wincon.h"
#include "ddk/ntddk.h"
#include "kernel_private.h"
#include "fileapi.h"

#include "wine/exception.h"
#include "wine/unicode.h"
#include "wine/debug.h"

// FIXME
#include "port.h"

WINE_DEFAULT_DEBUG_CHANNEL(file);

/* info structure for FindFirstFile handle */
typedef struct
{
    DWORD             magic;       /* magic number */
    HANDLE            handle;      /* handle to directory */
    CRITICAL_SECTION  cs;          /* crit section protecting this structure */
    FINDEX_SEARCH_OPS search_op;   /* Flags passed to FindFirst.  */
    FINDEX_INFO_LEVELS level;      /* Level passed to FindFirst */
    UNICODE_STRING    path;        /* NT path used to open the directory */
    BOOL              is_root;     /* is directory the root of the drive? */
    BOOL              wildcard;    /* did the mask contain wildcard characters? */
    UINT              data_pos;    /* current position in dir data */
    UINT              data_len;    /* length of dir data */
    UINT              data_size;   /* size of data buffer, or 0 when everything has been read */
    BYTE              data[1];     /* directory data */
} FIND_FIRST_INFO;

#define FIND_FIRST_MAGIC  0xc0ffee11

static const UINT max_entry_size = offsetof( FILE_BOTH_DIRECTORY_INFORMATION, FileName[256] );

static BOOL oem_file_apis;

static const WCHAR wildcardsW[] = { '*','?',0 };


/***********************************************************************
 *           FILE_name_AtoW
 *
 * Convert a file name to Unicode, taking into account the OEM/Ansi API mode.
 *
 * If alloc is FALSE uses the TEB static buffer, so it can only be used when
 * there is no possibility for the function to do that twice, taking into
 * account any called function.
 */
WCHAR *FILE_name_AtoW( LPCSTR name, BOOL alloc )
{
    ANSI_STRING str;
    UNICODE_STRING strW, *pstrW;
    NTSTATUS status;

    RtlInitAnsiString( &str, name );
    pstrW = alloc ? &strW : &NtCurrentTeb()->StaticUnicodeString;
    if (oem_file_apis)
        status = RtlOemStringToUnicodeString( pstrW, &str, alloc );
    else
        status = RtlAnsiStringToUnicodeString( pstrW, &str, alloc );
    if (status == STATUS_SUCCESS) return pstrW->Buffer;

    if (status == STATUS_BUFFER_OVERFLOW)
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
    else
        SetLastError( RtlNtStatusToDosError(status) );
    return NULL;
}


/***********************************************************************
 *           FILE_name_WtoA
 *
 * Convert a file name back to OEM/Ansi. Returns number of bytes copied.
 */
DWORD FILE_name_WtoA( LPCWSTR src, INT srclen, LPSTR dest, INT destlen )
{
    DWORD ret;

    if (srclen < 0) srclen = strlenW( src ) + 1;
    if (oem_file_apis)
        RtlUnicodeToOemN( dest, destlen, &ret, src, srclen * sizeof(WCHAR) );
    else
        RtlUnicodeToMultiByteN( dest, destlen, &ret, src, srclen * sizeof(WCHAR) );
    return ret;
}



/**************************************************************************
 *                      Operations on file names                          *
 **************************************************************************/


/*************************************************************************
 * CreateFileW [KERNEL32.@]  Creates or opens a file or other object
 *
 * Creates or opens an object, and returns a handle that can be used to
 * access that object.
 *
 * PARAMS
 *
 * filename     [in] pointer to filename to be accessed
 * access       [in] access mode requested
 * sharing      [in] share mode
 * sa           [in] pointer to security attributes
 * creation     [in] how to create the file
 * attributes   [in] attributes for newly created file
 * template     [in] handle to file with extended attributes to copy
 *
 * RETURNS
 *   Success: Open handle to specified file
 *   Failure: INVALID_HANDLE_VALUE
 */

/*
HANDLE WINAPI CreateFileW( LPCWSTR filename, DWORD access, DWORD sharing,
                              LPSECURITY_ATTRIBUTES sa, DWORD creation,
                              DWORD attributes, HANDLE template )
*/
// from far2l

HANDLE WINAPI CreateFileW( LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
		LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, 
		DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
		//return CreateFile( lpFileName, dwDesiredAccess, dwShareMode,
		//	lpSecurityAttributes, dwCreationDisposition, 
		//	dwFlagsAndAttributes, hTemplateFile);
		int flags = 0;
		if (dwDesiredAccess & (GENERIC_WRITE|GENERIC_ALL|FILE_WRITE_DATA|FILE_WRITE_ATTRIBUTES)) flags = O_RDWR;
		else if (dwDesiredAccess & (GENERIC_READ|GENERIC_ALL|FILE_READ_DATA|FILE_READ_ATTRIBUTES)) flags = O_RDONLY;
#ifdef _WIN32
		flags|= O_BINARY;
#else		
		flags|= O_CLOEXEC;
#ifdef __linux__
		if ((dwFlagsAndAttributes & FILE_FLAG_WRITE_THROUGH) != 0)
			flags|= O_DSYNC;

		if ((dwFlagsAndAttributes & FILE_FLAG_NO_BUFFERING) != 0)
			flags|= O_DIRECT;
		
#endif
#endif
		switch (dwCreationDisposition) 
		{
		case CREATE_ALWAYS: flags|= O_CREAT | O_TRUNC; break;
		case CREATE_NEW: flags|= O_CREAT | O_EXCL; break;
		case OPEN_ALWAYS: flags|= O_CREAT; break;
		case OPEN_EXISTING: break;
		case TRUNCATE_EXISTING: flags|= O_TRUNC; break;
		}
		const char *path = ConsumeWinPath(lpFileName);
		mode_t mode = (dwFlagsAndAttributes&FILE_ATTRIBUTE_EXECUTABLE) ? 0755 : 0644;
		// FIXME: os_call_int looks like wineserver call. code from ReactOS?
		//int r = os_call_int(open_all_args, path, flags, mode);
		int r = open(path, flags, mode);
		if (r==-1) {
			TranslateErrno();
/*
			fprintf(stderr, "CreateFile: " WS_FMT " - dwDesiredAccess=0x%x flags=0x%x path=%s errno=%d\n", 
				lpFileName, dwDesiredAccess, flags, path, errno);
*/
			return INVALID_HANDLE_VALUE;
		}

#ifndef __linux__
		if ((dwFlagsAndAttributes & (FILE_FLAG_WRITE_THROUGH|FILE_FLAG_NO_BUFFERING)) != 0) {
#ifdef __FreeBSD__
			fcntl(r, O_DIRECT, 1);
#else
			fcntl(r, F_NOCACHE, 1);
#endif // __FreeBSD__
		}
#endif // __linux__

		/*nobody cares.. if ((dwFlagsAndAttributes&FILE_FLAG_BACKUP_SEMANTICS)==0) {
			struct stat s = { };
			sdc_fstat(r, &s);
			if ( (s.st_mode & S_IFMT) == FILE_ATTRIBUTE_DIRECTORY) {
				sdc_close(r);
				WINPORT(SetLastError)(ERROR_DIRECTORY);
				return INVALID_HANDLE_VALUE;
			}
		}*/
		// TODO:
		//return WinPortHandle_Register(new WinPortHandleFile(r));
		return (HANDLE)r;
}

// from wine
/*************************************************************************
 *              CreateFileA              (KERNEL32.@)
 *
 * See CreateFileW.
 */
HANDLE WINAPI CreateFileA( LPCSTR filename, DWORD access, DWORD sharing,
                           LPSECURITY_ATTRIBUTES sa, DWORD creation,
                           DWORD attributes, HANDLE template)
{
    WCHAR *nameW;

    // FIXME
    //if ((GetVersion() & 0x80000000) && IsBadStringPtrA(filename, -1)) return INVALID_HANDLE_VALUE;
    if (!(nameW = FILE_name_AtoW( filename, FALSE ))) return INVALID_HANDLE_VALUE;
    return CreateFileW( nameW, access, sharing, sa, creation, attributes, template );
}
