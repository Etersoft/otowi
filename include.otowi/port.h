#ifndef __WINE_PORT_H
#define __WINE_PORT_H

// FIXME:
// Unknown defs from far2l:
#define FILE_ATTRIBUTE_EXECUTABLE           0x00400000

# define WS_FMT	"%ws"

// FIXME
#ifndef FIXME
#define FIXME
#endif

#ifndef TRACE
#define TRACE
#endif

// internal
extern VOID TranslateErrno();
extern const char * ConsumeWinPath( LPCWSTR lpFileName );

#endif  /* #ifdef __WINE_PORT_H */
