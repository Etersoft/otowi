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
