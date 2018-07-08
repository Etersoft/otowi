#ifndef __WINE_PORT_H
#define __WINE_PORT_H

// FIXME:
// Unknown defs from far2l:
//#define FILE_ATTRIBUTE_EXECUTABLE           0x00400000

# define WS_FMT	"%ws"

// internal
extern VOID TranslateErrno();

#endif  /* #ifdef __WINE_PORT_H */
