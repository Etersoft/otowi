/*
 * Win32 heap functions
 *
 * Copyright 1996 Alexandre Julliard
 * Copyright 1998 Ulrich Weigand
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
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#else
#define RUNNING_ON_VALGRIND 0
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#define NONAMELESSUNION
#include "windef.h"
#include "winnt.h"
#include "winternl.h"
#include "ntdll_misc.h"
#include "wine/list.h"
#include "wine/debug.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(heap);

/* Note: the heap data structures are loosely based on what Pietrek describes in his
 * book 'Windows 95 System Programming Secrets', with some adaptations for
 * better compatibility with NT.
 */

typedef struct tagARENA_INUSE
{
    DWORD  size;                    /* Block size; must be the first field */
    DWORD  magic : 24;              /* Magic number */
    DWORD  unused_bytes : 8;        /* Number of bytes in the block not used by user data (max value is HEAP_MIN_DATA_SIZE+HEAP_MIN_SHRINK_SIZE) */
} ARENA_INUSE;

typedef struct tagARENA_FREE
{
    DWORD                 size;     /* Block size; must be the first field */
    DWORD                 magic;    /* Magic number */
    struct list           entry;    /* Entry in free list */
} ARENA_FREE;

typedef struct
{
    struct list           entry;      /* entry in heap large blocks list */
    SIZE_T                data_size;  /* size of user data */
    SIZE_T                block_size; /* total size of virtual memory block */
    DWORD                 pad[2];     /* padding to ensure 16-byte alignment of data */
    DWORD                 size;       /* fields for compatibility with normal arenas */
    DWORD                 magic;      /* these must remain at the end of the structure */
} ARENA_LARGE;

#define ARENA_FLAG_FREE        0x00000001  /* flags OR'ed with arena size */
#define ARENA_FLAG_PREV_FREE   0x00000002
#define ARENA_SIZE_MASK        (~3)
#define ARENA_LARGE_SIZE       0xfedcba90  /* magic value for 'size' field in large blocks */

/* Value for arena 'magic' field */
#define ARENA_INUSE_MAGIC      0x455355
#define ARENA_PENDING_MAGIC    0xbedead
#define ARENA_FREE_MAGIC       0x45455246
#define ARENA_LARGE_MAGIC      0x6752614c

#define ARENA_INUSE_FILLER     0x55
#define ARENA_TAIL_FILLER      0xab
#define ARENA_FREE_FILLER      0xfeeefeee

/* everything is aligned on 8 byte boundaries (16 for Win64) */
#define ALIGNMENT              (2*sizeof(void*))
#define LARGE_ALIGNMENT        16  /* large blocks have stricter alignment */
#define ARENA_OFFSET           (ALIGNMENT - sizeof(ARENA_INUSE))

C_ASSERT( sizeof(ARENA_LARGE) % LARGE_ALIGNMENT == 0 );

#define ROUND_SIZE(size)       ((((size) + ALIGNMENT - 1) & ~(ALIGNMENT-1)) + ARENA_OFFSET)

#define QUIET                  1           /* Suppress messages  */
#define NOISY                  0           /* Report all errors  */

/* minimum data size (without arenas) of an allocated block */
/* make sure that it's larger than a free list entry */
#define HEAP_MIN_DATA_SIZE    ROUND_SIZE(2 * sizeof(struct list))
#define HEAP_MIN_ARENA_SIZE   (HEAP_MIN_DATA_SIZE + sizeof(ARENA_INUSE))
/* minimum size that must remain to shrink an allocated block */
#define HEAP_MIN_SHRINK_SIZE  (HEAP_MIN_DATA_SIZE+sizeof(ARENA_FREE))
/* minimum size to start allocating large blocks */
#define HEAP_MIN_LARGE_BLOCK_SIZE  0x7f000
/* extra size to add at the end of block for tail checking */
#define HEAP_TAIL_EXTRA_SIZE(flags) \
    ((flags & HEAP_TAIL_CHECKING_ENABLED) || RUNNING_ON_VALGRIND ? ALIGNMENT : 0)

/* There will be a free list bucket for every arena size up to and including this value */
#define HEAP_MAX_SMALL_FREE_LIST 0x100
C_ASSERT( HEAP_MAX_SMALL_FREE_LIST % ALIGNMENT == 0 );
#define HEAP_NB_SMALL_FREE_LISTS (((HEAP_MAX_SMALL_FREE_LIST - HEAP_MIN_ARENA_SIZE) / ALIGNMENT) + 1)

/* Max size of the blocks on the free lists above HEAP_MAX_SMALL_FREE_LIST */
static const SIZE_T HEAP_freeListSizes[] =
{
    0x200, 0x400, 0x1000, ~0UL
};
#define HEAP_NB_FREE_LISTS (sizeof(HEAP_freeListSizes) / sizeof(HEAP_freeListSizes[0]) + HEAP_NB_SMALL_FREE_LISTS)

typedef union
{
    ARENA_FREE  arena;
    void       *alignment[4];
} FREE_LIST_ENTRY;

struct tagHEAP;

typedef struct tagSUBHEAP
{
    void               *base;       /* Base address of the sub-heap memory block */
    SIZE_T              size;       /* Size of the whole sub-heap */
    SIZE_T              min_commit; /* Minimum committed size */
    SIZE_T              commitSize; /* Committed size of the sub-heap */
    struct list         entry;      /* Entry in sub-heap list */
    struct tagHEAP     *heap;       /* Main heap structure */
    DWORD               headerSize; /* Size of the heap header */
    DWORD               magic;      /* Magic number */
} SUBHEAP;

#define SUBHEAP_MAGIC    ((DWORD)('S' | ('U'<<8) | ('B'<<16) | ('H'<<24)))

typedef struct tagHEAP
{
    DWORD_PTR        unknown1[2];
    DWORD            unknown2;
    DWORD            flags;         /* Heap flags */
    DWORD            force_flags;   /* Forced heap flags for debugging */
    SUBHEAP          subheap;       /* First sub-heap */
    struct list      entry;         /* Entry in process heap list */
    struct list      subheap_list;  /* Sub-heap list */
    struct list      large_list;    /* Large blocks list */
    SIZE_T           grow_size;     /* Size of next subheap for growing heap */
    DWORD            magic;         /* Magic number */
    DWORD            pending_pos;   /* Position in pending free requests ring */
    ARENA_INUSE    **pending_free;  /* Ring buffer for pending free requests */
    RTL_CRITICAL_SECTION critSection; /* Critical section for serialization */
    FREE_LIST_ENTRY *freeList;      /* Free lists */
} HEAP;

#define HEAP_MAGIC       ((DWORD)('H' | ('E'<<8) | ('A'<<16) | ('P'<<24)))

#define HEAP_DEF_SIZE        0x110000   /* Default heap size = 1Mb + 64Kb */
#define COMMIT_MASK          0xffff  /* bitmask for commit/decommit granularity */
#define MAX_FREE_PENDING     1024    /* max number of free requests to delay */

/* some undocumented flags (names are made up) */
#define HEAP_PAGE_ALLOCS      0x01000000
#define HEAP_VALIDATE         0x10000000
#define HEAP_VALIDATE_ALL     0x20000000
#define HEAP_VALIDATE_PARAMS  0x40000000



/***********************************************************************
 *           RtlCreateHeap   (NTDLL.@)
 *
 * Create a new Heap.
 *
 * PARAMS
 *  flags      [I] HEAP_ flags from "winnt.h"
 *  addr       [I] Desired base address
 *  totalSize  [I] Total size of the heap, or 0 for a growable heap
 *  commitSize [I] Amount of heap space to commit
 *  unknown    [I] Not yet understood
 *  definition [I] Heap definition
 *
 * RETURNS
 *  Success: A HANDLE to the newly created heap.
 *  Failure: a NULL HANDLE.
 */
HANDLE WINAPI RtlCreateHeap( ULONG flags, PVOID addr, SIZE_T totalSize, SIZE_T commitSize,
                             PVOID unknown, PRTL_HEAP_DEFINITION definition )
{
    SUBHEAP *subheap;

    /* Allocate the heap block */

    if (!totalSize)
    {
        totalSize = HEAP_DEF_SIZE;
        flags |= HEAP_GROWABLE;
    }

    return (HANDLE)0xcece;
}


/***********************************************************************
 *           RtlDestroyHeap   (NTDLL.@)
 *
 * Destroy a Heap created with RtlCreateHeap().
 *
 * PARAMS
 *  heap [I] Heap to destroy.
 *
 * RETURNS
 *  Success: A NULL HANDLE, if heap is NULL or it was destroyed
 *  Failure: The Heap handle, if heap is the process heap.
 */
HANDLE WINAPI RtlDestroyHeap( HANDLE heap )
{

    TRACE("%p\n", heap );
    if (heap) return heap;

    return 0;
}


/***********************************************************************
 *           RtlAllocateHeap   (NTDLL.@)
 *
 * Allocate a memory block from a Heap.
 *
 * PARAMS
 *  heap  [I] Heap to allocate block from
 *  flags [I] HEAP_ flags from "winnt.h"
 *  size  [I] Size of the memory block to allocate
 *
 * RETURNS
 *  Success: A pointer to the newly allocated block
 *  Failure: NULL.
 *
 * NOTES
 *  This call does not SetLastError().
 */
PVOID WINAPI RtlAllocateHeap( HANDLE heap, ULONG flags, SIZE_T size )
{
    ARENA_FREE *pArena;
    ARENA_INUSE *pInUse;
    SUBHEAP *subheap;
    //HEAP *heapPtr = HEAP_GetPtr( heap );
    SIZE_T rounded_size;

    /* Validate the parameters */

    //if (!heap) return NULL;
    flags &= HEAP_GENERATE_EXCEPTIONS | HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY;
    //flags |= heapPtr->flags;
    rounded_size = ROUND_SIZE(size) + HEAP_TAIL_EXTRA_SIZE( flags );
    if (rounded_size < size)  /* overflow */
    {
        if (flags & HEAP_GENERATE_EXCEPTIONS) RtlRaiseStatus( STATUS_NO_MEMORY );
        return NULL;
    }
    if (rounded_size < HEAP_MIN_DATA_SIZE) rounded_size = HEAP_MIN_DATA_SIZE;

    return malloc(rounded_size);
}


/***********************************************************************
 *           RtlFreeHeap   (NTDLL.@)
 *
 * Free a memory block allocated with RtlAllocateHeap().
 *
 * PARAMS
 *  heap  [I] Heap that block was allocated from
 *  flags [I] HEAP_ flags from "winnt.h"
 *  ptr   [I] Block to free
 *
 * RETURNS
 *  Success: TRUE, if ptr is NULL or was freed successfully.
 *  Failure: FALSE.
 */
BOOLEAN WINAPI RtlFreeHeap( HANDLE heap, ULONG flags, PVOID ptr )
{
    /* Validate the parameters */

    if (!ptr) return TRUE;  /* freeing a NULL ptr isn't an error in Win2k */

    free(ptr);
    return TRUE;
}


/***********************************************************************
 *           RtlReAllocateHeap   (NTDLL.@)
 *
 * Change the size of a memory block allocated with RtlAllocateHeap().
 *
 * PARAMS
 *  heap  [I] Heap that block was allocated from
 *  flags [I] HEAP_ flags from "winnt.h"
 *  ptr   [I] Block to resize
 *  size  [I] Size of the memory block to allocate
 *
 * RETURNS
 *  Success: A pointer to the resized block (which may be different).
 *  Failure: NULL.
 */
PVOID WINAPI RtlReAllocateHeap( HANDLE heap, ULONG flags, PVOID ptr, SIZE_T size )
{
    ARENA_INUSE *pArena;
    SUBHEAP *subheap;
    SIZE_T oldBlockSize, oldActualSize, rounded_size;
    void *ret;

    if (!ptr) return NULL;

    /* Validate the parameters */

    flags &= HEAP_GENERATE_EXCEPTIONS | HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY |
             HEAP_REALLOC_IN_PLACE_ONLY;
    //flags |= heapPtr->flags;
    //if (!(flags & HEAP_NO_SERIALIZE)) RtlEnterCriticalSection( &heapPtr->critSection );

    rounded_size = ROUND_SIZE(size) + HEAP_TAIL_EXTRA_SIZE(flags);
    if (rounded_size < size) goto oom;  /* overflow */
    if (rounded_size < HEAP_MIN_DATA_SIZE) rounded_size = HEAP_MIN_DATA_SIZE;

    FIXME("TODO: need copy in new place?");
    ret = realloc(ptr, size);
done:
    //if (!(flags & HEAP_NO_SERIALIZE)) RtlLeaveCriticalSection( &heapPtr->critSection );
    TRACE("(%p,%08x,%p,%08lx): returning %p\n", heap, flags, ptr, size, ret );
    return ret;

oom:
    //if (!(flags & HEAP_NO_SERIALIZE)) RtlLeaveCriticalSection( &heapPtr->critSection );
    if (flags & HEAP_GENERATE_EXCEPTIONS) RtlRaiseStatus( STATUS_NO_MEMORY );
    RtlSetLastWin32ErrorAndNtStatusFromNtStatus( STATUS_NO_MEMORY );
    TRACE("(%p,%08x,%p,%08lx): returning NULL\n", heap, flags, ptr, size );
    return NULL;

error:
    //if (!(flags & HEAP_NO_SERIALIZE)) RtlLeaveCriticalSection( &heapPtr->critSection );
    RtlSetLastWin32ErrorAndNtStatusFromNtStatus( STATUS_INVALID_PARAMETER );
    TRACE("(%p,%08x,%p,%08lx): returning NULL\n", heap, flags, ptr, size );
    return NULL;
}


/***********************************************************************
 *           RtlCompactHeap   (NTDLL.@)
 *
 * Compact the free space in a Heap.
 *
 * PARAMS
 *  heap  [I] Heap that block was allocated from
 *  flags [I] HEAP_ flags from "winnt.h"
 *
 * RETURNS
 *  The number of bytes compacted.
 *
 * NOTES
 *  This function is a harmless stub.
 */
ULONG WINAPI RtlCompactHeap( HANDLE heap, ULONG flags )
{
    static BOOL reported;
    if (!reported++) FIXME( "(%p, 0x%x) stub\n", heap, flags );
    return 0;
}


/***********************************************************************
 *           RtlLockHeap   (NTDLL.@)
 *
 * Lock a Heap.
 *
 * PARAMS
 *  heap  [I] Heap to lock
 *
 * RETURNS
 *  Success: TRUE. The Heap is locked.
 *  Failure: FALSE, if heap is invalid.
 */
BOOLEAN WINAPI RtlLockHeap( HANDLE heap )
{
    return TRUE;
}


/***********************************************************************
 *           RtlUnlockHeap   (NTDLL.@)
 *
 * Unlock a Heap.
 *
 * PARAMS
 *  heap  [I] Heap to unlock
 *
 * RETURNS
 *  Success: TRUE. The Heap is unlocked.
 *  Failure: FALSE, if heap is invalid.
 */
BOOLEAN WINAPI RtlUnlockHeap( HANDLE heap )
{
    return TRUE;
}


/***********************************************************************
 *           RtlSizeHeap   (NTDLL.@)
 *
 * Get the actual size of a memory block allocated from a Heap.
 *
 * PARAMS
 *  heap  [I] Heap that block was allocated from
 *  flags [I] HEAP_ flags from "winnt.h"
 *  ptr   [I] Block to get the size of
 *
 * RETURNS
 *  Success: The size of the block.
 *  Failure: -1, heap or ptr are invalid.
 *
 * NOTES
 *  The size may be bigger than what was passed to RtlAllocateHeap().
 */
SIZE_T WINAPI RtlSizeHeap( HANDLE heap, ULONG flags, const void *ptr )
{
    SIZE_T ret;

    // TODO
    ret = 1;

    TRACE("(%p,%08x,%p): returning %08lx\n", heap, flags, ptr, ret );
    return ret;
}


/***********************************************************************
 *           RtlValidateHeap   (NTDLL.@)
 *
 * Determine if a block is a valid allocation from a heap.
 *
 * PARAMS
 *  heap  [I] Heap that block was allocated from
 *  flags [I] HEAP_ flags from "winnt.h"
 *  ptr   [I] Block to check
 *
 * RETURNS
 *  Success: TRUE. The block was allocated from heap.
 *  Failure: FALSE, if heap is invalid or ptr was not allocated from it.
 */
BOOLEAN WINAPI RtlValidateHeap( HANDLE heap, ULONG flags, LPCVOID ptr )
{
    return TRUE;
}


/***********************************************************************
 *           RtlWalkHeap    (NTDLL.@)
 *
 * FIXME
 *  The PROCESS_HEAP_ENTRY flag values seem different between this
 *  function and HeapWalk(). To be checked.
 */
NTSTATUS WINAPI RtlWalkHeap( HANDLE heap, PVOID entry_ptr )
{
    NTSTATUS ret;
    ret = STATUS_SUCCESS;
    return ret;
}


/***********************************************************************
 *           RtlGetProcessHeaps    (NTDLL.@)
 *
 * Get the Heaps belonging to the current process.
 *
 * PARAMS
 *  count [I] size of heaps
 *  heaps [O] Destination array for heap HANDLE's
 *
 * RETURNS
 *  Success: The number of Heaps allocated by the process.
 *  Failure: 0.
 */
ULONG WINAPI RtlGetProcessHeaps( ULONG count, HANDLE *heaps )
{
    ULONG total = 1;  /* main heap */
    return total;
}

/***********************************************************************
 *           RtlQueryHeapInformation    (NTDLL.@)
 */
NTSTATUS WINAPI RtlQueryHeapInformation( HANDLE heap, HEAP_INFORMATION_CLASS info_class,
                                         PVOID info, SIZE_T size_in, PSIZE_T size_out)
{
    switch (info_class)
    {
    case HeapCompatibilityInformation:
        if (size_out) *size_out = sizeof(ULONG);

        if (size_in < sizeof(ULONG))
            return STATUS_BUFFER_TOO_SMALL;

        *(ULONG *)info = 0; /* standard heap */
        return STATUS_SUCCESS;

    default:
        FIXME("Unknown heap information class %u\n", info_class);
        return STATUS_INVALID_INFO_CLASS;
    }
}

/***********************************************************************
 *           RtlSetHeapInformation    (NTDLL.@)
 */
NTSTATUS WINAPI RtlSetHeapInformation( HANDLE heap, HEAP_INFORMATION_CLASS info_class, PVOID info, SIZE_T size)
{
    FIXME("%p %d %p %ld stub\n", heap, info_class, info, size);
    return STATUS_SUCCESS;
}
