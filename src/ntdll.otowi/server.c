/*
 * Wine server communication
 *
 * Copyright (C) 1998 Alexandre Julliard
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
#include <ctype.h>
#ifdef HAVE_DIRENT_H
# include <dirent.h>
#endif
#include <errno.h>
#include <fcntl.h>
//#ifdef HAVE_LWP_H
//#include <lwp.h>
//#endif
#ifdef HAVE_PTHREAD_NP_H
# include <pthread_np.h>
#endif
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_PRCTL_H
# include <sys/prctl.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_SYSCALL_H
# include <sys/syscall.h>
#endif
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#ifdef HAVE_SYS_UCONTEXT_H
# include <sys/ucontext.h>
#endif
#ifdef HAVE_SYS_THR_H
#include <sys/thr.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winnt.h"
#include "wine/library.h"
#include "wine/server.h"
#include "wine/debug.h"
#include "ntdll_misc.h"

WINE_DEFAULT_DEBUG_CHANNEL(server);


/***********************************************************************
 *           wine_server_call (NTDLL.@)
 *
 * Perform a server call.
 *
 * PARAMS
 *     req_ptr [I/O] Function dependent data
 *
 * RETURNS
 *     Depends on server function being called, but usually an NTSTATUS code.
 *
 * NOTES
 *     Use the SERVER_START_REQ and SERVER_END_REQ to help you fill out the
 *     server request structure for the particular call. E.g:
 *|     SERVER_START_REQ( event_op )
 *|     {
 *|         req->handle = handle;
 *|         req->op     = SET_EVENT;
 *|         ret = wine_server_call( req );
 *|     }
 *|     SERVER_END_REQ;
 */
unsigned int wine_server_call( void *req_ptr )
{
    FIXME("");
    return 0;
#if 0
    sigset_t old_set;
    unsigned int ret;

    pthread_sigmask( SIG_BLOCK, &server_block_set, &old_set );
    ret = server_call_unlocked( req_ptr );
    pthread_sigmask( SIG_SETMASK, &old_set, NULL );
    return ret;
#endif
}

/***********************************************************************
 *              server_select
 */
unsigned int server_select( const select_op_t *select_op, data_size_t size, UINT flags,
                            const LARGE_INTEGER *timeout )
{
    FIXME("");
    return STATUS_TIMEOUT;
}
