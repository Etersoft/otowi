/*
 * Locale support
 *
 * Copyright 1995 Martin von Loewis
 * Copyright 1998 David Lee Lambert
 * Copyright 2000 Julio César Gázquez
 * Copyright 2002 Alexandre Julliard for CodeWeavers
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
#include <locale.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#ifdef __APPLE__
# include <CoreFoundation/CFLocale.h>
# include <CoreFoundation/CFString.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winuser.h"  /* for RT_STRINGW */
#include "winternl.h"
#include "wine/unicode.h"
#include "winnls.h"
#include "winerror.h"
#include "winver.h"
#include "kernel_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(nls);

#define LOCALE_LOCALEINFOFLAGSMASK (LOCALE_NOUSEROVERRIDE|LOCALE_USE_CP_ACP|\
                                    LOCALE_RETURN_NUMBER|LOCALE_RETURN_GENITIVE_NAMES)
#define MB_FLAGSMASK (MB_PRECOMPOSED|MB_COMPOSITE|MB_USEGLYPHCHARS|MB_ERR_INVALID_CHARS)
#define WC_FLAGSMASK (WC_DISCARDNS|WC_SEPCHARS|WC_DEFAULTCHAR|WC_ERR_INVALID_CHARS|\
                      WC_COMPOSITECHECK|WC_NO_BEST_FIT_CHARS)

/* current code pages */
static const union cptable *ansi_cptable;
static const union cptable *oem_cptable;
static const union cptable *mac_cptable;
static const union cptable *unix_cptable;  /* NULL if UTF8 */


/******************************************************************************
 *		LOCALE_Init
 */
void LOCALE_Init(void)
{
    extern void CDECL __wine_init_codepages( const union cptable *ansi_cp, const union cptable *oem_cp,
                                             const union cptable *unix_cp );

    UINT ansi_cp = 1252, oem_cp = 437, mac_cp = 10000, unix_cp;

    setlocale( LC_ALL, "" );

    unix_cp = CP_UTF8;
    // unix_cp = setup_unix_locales();
    //if (!lcid_LC_MESSAGES) lcid_LC_MESSAGES = lcid_LC_CTYPE;

#ifdef __APPLE__
    if (!unix_cp)
        unix_cp = CP_UTF8;  /* default to utf-8 even if we don't get a valid locale */
#endif

    if (!(ansi_cptable = wine_cp_get_table( ansi_cp )))
        ansi_cptable = wine_cp_get_table( 1252 );
    if (!(oem_cptable = wine_cp_get_table( oem_cp )))
        oem_cptable  = wine_cp_get_table( 437 );
    if (!(mac_cptable = wine_cp_get_table( mac_cp )))
        mac_cptable  = wine_cp_get_table( 10000 );
    if (unix_cp != CP_UTF8)
    {
        if (!(unix_cptable = wine_cp_get_table( unix_cp )))
            unix_cptable  = wine_cp_get_table( 28591 );
    }

    __wine_init_codepages( ansi_cptable, oem_cptable, unix_cptable );

    TRACE( "ansi=%03d oem=%03d mac=%03d unix=%03d\n",
           ansi_cptable->info.codepage, oem_cptable->info.codepage,
           mac_cptable->info.codepage, unix_cp );

    //setlocale(LC_NUMERIC, "C");  /* FIXME: oleaut32 depends on this */
}

/***********************************************************************
 *		get_lcid_codepage
 *
 * Retrieve the ANSI codepage for a given locale.
 */
static inline UINT get_lcid_codepage( LCID lcid )
{
    UINT ret;
    ret  = 0;
//    if (!GetLocaleInfoW( lcid, LOCALE_IDEFAULTANSICODEPAGE|LOCALE_RETURN_NUMBER, (WCHAR *)&ret,
//                         sizeof(ret)/sizeof(WCHAR) )) ret = 0;
    return ret;
}


/******************************************************************************
 *              GetACP   (KERNEL32.@)
 *
 * Get the current Ansi code page Id for the system.
 *
 * PARAMS
 *  None.
 *
 * RETURNS
 *    The current Ansi code page identifier for the system.
 */
UINT WINAPI GetACP(void)
{
    assert( ansi_cptable );
    return ansi_cptable->info.codepage;
}


/******************************************************************************
 *              SetCPGlobal   (KERNEL32.@)
 *
 * Set the current Ansi code page Id for the system.
 *
 * PARAMS
 *    acp [I] code page ID to be the new ACP.
 *
 * RETURNS
 *    The previous ACP.
 */
UINT WINAPI SetCPGlobal( UINT acp )
{
    UINT ret = GetACP();
    const union cptable *new_cptable = wine_cp_get_table( acp );

    if (new_cptable) ansi_cptable = new_cptable;
    return ret;
}


/***********************************************************************
 *              GetOEMCP   (KERNEL32.@)
 *
 * Get the current OEM code page Id for the system.
 *
 * PARAMS
 *  None.
 *
 * RETURNS
 *    The current OEM code page identifier for the system.
 */
UINT WINAPI GetOEMCP(void)
{
    assert( oem_cptable );
    return oem_cptable->info.codepage;
}


/***********************************************************************
 *           IsValidCodePage   (KERNEL32.@)
 *
 * Determine if a given code page identifier is valid.
 *
 * PARAMS
 *  codepage [I] Code page Id to verify.
 *
 * RETURNS
 *  TRUE, If codepage is valid and available on the system,
 *  FALSE otherwise.
 */
BOOL WINAPI IsValidCodePage( UINT codepage )
{
    switch(codepage) {
    case CP_UTF7:
    case CP_UTF8:
        return TRUE;
    default:
        return wine_cp_get_table( codepage ) != NULL;
    }
}


/***********************************************************************
 *		get_codepage_table
 *
 * Find the table for a given codepage, handling CP_ACP etc. pseudo-codepages
 */
static const union cptable *get_codepage_table( unsigned int codepage )
{
    const union cptable *ret = NULL;

    assert( ansi_cptable );  /* init must have been done already */

    switch(codepage)
    {
    case CP_ACP:
        return ansi_cptable;
    case CP_OEMCP:
        return oem_cptable;
    case CP_MACCP:
        return mac_cptable;
    case CP_UTF7:
    case CP_UTF8:
        break;
    case CP_THREAD_ACP:
        //if (NtCurrentTeb()->CurrentLocale == GetUserDefaultLCID()) return ansi_cptable;
        codepage = get_lcid_codepage( NtCurrentTeb()->CurrentLocale );
        if (!codepage) return ansi_cptable;
        /* fall through */
    default:
        if (codepage == ansi_cptable->info.codepage) return ansi_cptable;
        if (codepage == oem_cptable->info.codepage) return oem_cptable;
        if (codepage == mac_cptable->info.codepage) return mac_cptable;
        ret = wine_cp_get_table( codepage );
        break;
    }
    return ret;
}


/***********************************************************************
 *              utf7_write_w
 *
 * Helper for utf7_mbstowcs
 *
 * RETURNS
 *   TRUE on success, FALSE on error
 */
static inline BOOL utf7_write_w(WCHAR *dst, int dstlen, int *index, WCHAR character)
{
    if (dstlen > 0)
    {
        if (*index >= dstlen)
            return FALSE;

        dst[*index] = character;
    }

    (*index)++;

    return TRUE;
}


/***********************************************************************
 *              utf7_mbstowcs
 *
 * UTF-7 to UTF-16 string conversion, helper for MultiByteToWideChar
 *
 * RETURNS
 *   On success, the number of characters written
 *   On dst buffer overflow, -1
 */
static int utf7_mbstowcs(const char *src, int srclen, WCHAR *dst, int dstlen)
{
    static const signed char base64_decoding_table[] =
    {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0x00-0x0F */
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0x10-0x1F */
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, /* 0x20-0x2F */
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, /* 0x30-0x3F */
        -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, /* 0x40-0x4F */
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, /* 0x50-0x5F */
        -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, /* 0x60-0x6F */
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1  /* 0x70-0x7F */
    };

    const char *source_end = src + srclen;
    int dest_index = 0;

    DWORD byte_pair = 0;
    short offset = 0;

    while (src < source_end)
    {
        if (*src == '+')
        {
            src++;
            if (src >= source_end)
                break;

            if (*src == '-')
            {
                /* just a plus sign escaped as +- */
                if (!utf7_write_w(dst, dstlen, &dest_index, '+'))
                    return -1;
                src++;
                continue;
            }

            do
            {
                signed char sextet = *src;
                if (sextet == '-')
                {
                    /* skip over the dash and end base64 decoding
                     * the current, unfinished byte pair is discarded */
                    src++;
                    offset = 0;
                    break;
                }
                if (sextet < 0)
                {
                    /* the next character of src is < 0 and therefore not part of a base64 sequence
                     * the current, unfinished byte pair is NOT discarded in this case
                     * this is probably a bug in Windows */
                    break;
                }

                sextet = base64_decoding_table[sextet];
                if (sextet == -1)
                {
                    /* -1 means that the next character of src is not part of a base64 sequence
                     * in other words, all sextets in this base64 sequence have been processed
                     * the current, unfinished byte pair is discarded */
                    offset = 0;
                    break;
                }

                byte_pair = (byte_pair << 6) | sextet;
                offset += 6;

                if (offset >= 16)
                {
                    /* this byte pair is done */
                    if (!utf7_write_w(dst, dstlen, &dest_index, (byte_pair >> (offset - 16)) & 0xFFFF))
                        return -1;
                    offset -= 16;
                }

                src++;
            }
            while (src < source_end);
        }
        else
        {
            /* we have to convert to unsigned char in case *src < 0 */
            if (!utf7_write_w(dst, dstlen, &dest_index, (unsigned char)*src))
                return -1;
            src++;
        }
    }

    return dest_index;
}

/***********************************************************************
 *              MultiByteToWideChar   (KERNEL32.@)
 *
 * Convert a multibyte character string into a Unicode string.
 *
 * PARAMS
 *   page   [I] Codepage character set to convert from
 *   flags  [I] Character mapping flags
 *   src    [I] Source string buffer
 *   srclen [I] Length of src (in bytes), or -1 if src is NUL terminated
 *   dst    [O] Destination buffer
 *   dstlen [I] Length of dst (in WCHARs), or 0 to compute the required length
 *
 * RETURNS
 *   Success: If dstlen > 0, the number of characters written to dst.
 *            If dstlen == 0, the number of characters needed to perform the
 *            conversion. In both cases the count includes the terminating NUL.
 *   Failure: 0. Use GetLastError() to determine the cause. Possible errors are
 *            ERROR_INSUFFICIENT_BUFFER, if not enough space is available in dst
 *            and dstlen != 0; ERROR_INVALID_PARAMETER,  if an invalid parameter
 *            is passed, and ERROR_NO_UNICODE_TRANSLATION if no translation is
 *            possible for src.
 */
INT WINAPI MultiByteToWideChar( UINT page, DWORD flags, LPCSTR src, INT srclen,
                                LPWSTR dst, INT dstlen )
{
    const union cptable *table;
    int ret;

    if (!src || !srclen || (!dst && dstlen) || dstlen < 0)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }

    if (srclen < 0) srclen = strlen(src) + 1;

    switch(page)
    {
    case CP_SYMBOL:
        if (flags)
        {
            SetLastError( ERROR_INVALID_FLAGS );
            return 0;
        }
        ret = wine_cpsymbol_mbstowcs( src, srclen, dst, dstlen );
        break;
    case CP_UTF7:
        if (flags)
        {
            SetLastError( ERROR_INVALID_FLAGS );
            return 0;
        }
        ret = utf7_mbstowcs( src, srclen, dst, dstlen );
        break;
    case CP_UNIXCP:
        if (unix_cptable)
        {
            ret = wine_cp_mbstowcs( unix_cptable, flags, src, srclen, dst, dstlen );
            break;
        }
#ifdef __APPLE__
        flags |= MB_COMPOSITE;  /* work around broken Mac OS X filesystem that enforces decomposed Unicode */
#endif
        /* fall through */
    case CP_UTF8:
        if (flags & ~MB_FLAGSMASK)
        {
            SetLastError( ERROR_INVALID_FLAGS );
            return 0;
        }
        ret = wine_utf8_mbstowcs( flags, src, srclen, dst, dstlen );
        break;
    default:
        if (!(table = get_codepage_table( page )))
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return 0;
        }
        if (flags & ~MB_FLAGSMASK)
        {
            SetLastError( ERROR_INVALID_FLAGS );
            return 0;
        }
        ret = wine_cp_mbstowcs( table, flags, src, srclen, dst, dstlen );
        break;
    }

    if (ret < 0)
    {
        switch(ret)
        {
        case -1: SetLastError( ERROR_INSUFFICIENT_BUFFER ); break;
        case -2: SetLastError( ERROR_NO_UNICODE_TRANSLATION ); break;
        }
        ret = 0;
    }
    TRACE("cp %d %s -> %s, ret = %d\n",
          page, debugstr_an(src, srclen), debugstr_wn(dst, ret), ret);
    return ret;
}


/***********************************************************************
 *              utf7_can_directly_encode
 *
 * Helper for utf7_wcstombs
 */
static inline BOOL utf7_can_directly_encode(WCHAR codepoint)
{
    static const BOOL directly_encodable_table[] =
    {
        1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, /* 0x00 - 0x0F */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x10 - 0x1F */
        1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, /* 0x20 - 0x2F */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, /* 0x30 - 0x3F */
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x40 - 0x4F */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, /* 0x50 - 0x5F */
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x60 - 0x6F */
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1                 /* 0x70 - 0x7A */
    };

    return codepoint <= 0x7A ? directly_encodable_table[codepoint] : FALSE;
}

/***********************************************************************
 *              utf7_write_c
 *
 * Helper for utf7_wcstombs
 *
 * RETURNS
 *   TRUE on success, FALSE on error
 */
static inline BOOL utf7_write_c(char *dst, int dstlen, int *index, char character)
{
    if (dstlen > 0)
    {
        if (*index >= dstlen)
            return FALSE;

        dst[*index] = character;
    }

    (*index)++;

    return TRUE;
}

/***********************************************************************
 *              utf7_wcstombs
 *
 * UTF-16 to UTF-7 string conversion, helper for WideCharToMultiByte
 *
 * RETURNS
 *   On success, the number of characters written
 *   On dst buffer overflow, -1
 */
static int utf7_wcstombs(const WCHAR *src, int srclen, char *dst, int dstlen)
{
    static const char base64_encoding_table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    const WCHAR *source_end = src + srclen;
    int dest_index = 0;

    while (src < source_end)
    {
        if (*src == '+')
        {
            if (!utf7_write_c(dst, dstlen, &dest_index, '+'))
                return -1;
            if (!utf7_write_c(dst, dstlen, &dest_index, '-'))
                return -1;
            src++;
        }
        else if (utf7_can_directly_encode(*src))
        {
            if (!utf7_write_c(dst, dstlen, &dest_index, *src))
                return -1;
            src++;
        }
        else
        {
            unsigned int offset = 0;
            DWORD byte_pair = 0;

            if (!utf7_write_c(dst, dstlen, &dest_index, '+'))
                return -1;

            while (src < source_end && !utf7_can_directly_encode(*src))
            {
                byte_pair = (byte_pair << 16) | *src;
                offset += 16;
                while (offset >= 6)
                {
                    if (!utf7_write_c(dst, dstlen, &dest_index, base64_encoding_table[(byte_pair >> (offset - 6)) & 0x3F]))
                        return -1;
                    offset -= 6;
                }
                src++;
            }

            if (offset)
            {
                /* Windows won't create a padded base64 character if there's no room for the - sign
                 * as well ; this is probably a bug in Windows */
                if (dstlen > 0 && dest_index + 1 >= dstlen)
                    return -1;

                byte_pair <<= (6 - offset);
                if (!utf7_write_c(dst, dstlen, &dest_index, base64_encoding_table[byte_pair & 0x3F]))
                    return -1;
            }

            /* Windows always explicitly terminates the base64 sequence
               even though RFC 2152 (page 3, rule 2) does not require this */
            if (!utf7_write_c(dst, dstlen, &dest_index, '-'))
                return -1;
        }
    }

    return dest_index;
}

/***********************************************************************
 *              WideCharToMultiByte   (KERNEL32.@)
 *
 * Convert a Unicode character string into a multibyte string.
 *
 * PARAMS
 *   page    [I] Code page character set to convert to
 *   flags   [I] Mapping Flags (MB_ constants from "winnls.h").
 *   src     [I] Source string buffer
 *   srclen  [I] Length of src (in WCHARs), or -1 if src is NUL terminated
 *   dst     [O] Destination buffer
 *   dstlen  [I] Length of dst (in bytes), or 0 to compute the required length
 *   defchar [I] Default character to use for conversion if no exact
 *		    conversion can be made
 *   used    [O] Set if default character was used in the conversion
 *
 * RETURNS
 *   Success: If dstlen > 0, the number of characters written to dst.
 *            If dstlen == 0, number of characters needed to perform the
 *            conversion. In both cases the count includes the terminating NUL.
 *   Failure: 0. Use GetLastError() to determine the cause. Possible errors are
 *            ERROR_INSUFFICIENT_BUFFER, if not enough space is available in dst
 *            and dstlen != 0, and ERROR_INVALID_PARAMETER, if an invalid
 *            parameter was given.
 */
INT WINAPI WideCharToMultiByte( UINT page, DWORD flags, LPCWSTR src, INT srclen,
                                LPSTR dst, INT dstlen, LPCSTR defchar, BOOL *used )
{
    const union cptable *table;
    int ret, used_tmp;

    if (!src || !srclen || (!dst && dstlen) || dstlen < 0)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }

    if (srclen < 0) srclen = strlenW(src) + 1;

    switch(page)
    {
    case CP_SYMBOL:
        /* when using CP_SYMBOL, ERROR_INVALID_FLAGS takes precedence */
        if (flags)
        {
            SetLastError( ERROR_INVALID_FLAGS );
            return 0;
        }
        if (defchar || used)
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return 0;
        }
        ret = wine_cpsymbol_wcstombs( src, srclen, dst, dstlen );
        break;
    case CP_UTF7:
        /* when using CP_UTF7, ERROR_INVALID_PARAMETER takes precedence */
        if (defchar || used)
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return 0;
        }
        if (flags)
        {
            SetLastError( ERROR_INVALID_FLAGS );
            return 0;
        }
        ret = utf7_wcstombs( src, srclen, dst, dstlen );
        break;
    case CP_UNIXCP:
        if (unix_cptable)
        {
            ret = wine_cp_wcstombs( unix_cptable, flags, src, srclen, dst, dstlen,
                                    defchar, used ? &used_tmp : NULL );
            break;
        }
        /* fall through */
    case CP_UTF8:
        if (defchar || used)
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return 0;
        }
        if (flags & ~WC_FLAGSMASK)
        {
            SetLastError( ERROR_INVALID_FLAGS );
            return 0;
        }
        ret = wine_utf8_wcstombs( flags, src, srclen, dst, dstlen );
        break;
    default:
        if (!(table = get_codepage_table( page )))
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return 0;
        }
        if (flags & ~WC_FLAGSMASK)
        {
            SetLastError( ERROR_INVALID_FLAGS );
            return 0;
        }
        ret = wine_cp_wcstombs( table, flags, src, srclen, dst, dstlen,
                                defchar, used ? &used_tmp : NULL );
        if (used) *used = used_tmp;
        break;
    }

    if (ret < 0)
    {
        switch(ret)
        {
        case -1: SetLastError( ERROR_INSUFFICIENT_BUFFER ); break;
        case -2: SetLastError( ERROR_NO_UNICODE_TRANSLATION ); break;
        }
        ret = 0;
    }
    TRACE("cp %d %s -> %s, ret = %d\n",
          page, debugstr_wn(src, srclen), debugstr_an(dst, ret), ret);
    return ret;
}
