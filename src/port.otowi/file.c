/* from far2l */

#include <stdio.h>
#include <errno.h>

#include "winerror.h"
#include "windef.h"
#include "winbase.h"

#include "port.h"

VOID TranslateErrno()
{
	DWORD gle;
	switch (errno) {
		case 0: gle = 0; break;
		case EEXIST: gle = ERROR_ALREADY_EXISTS; break;
		case ENOENT: gle = ERROR_FILE_NOT_FOUND; break;
		case EACCES: case EPERM: gle = ERROR_ACCESS_DENIED; break;
		case ETXTBSY: gle = ERROR_SHARING_VIOLATION; break;
		case EINVAL: gle = ERROR_INVALID_PARAMETER; break;
		//case EROFS: gle = ; break;
		default:
			gle = 20000 + errno;
			fprintf(stderr, "TODO: TranslateErrno - %d\n", errno );
	}
	
	SetLastError(gle);
}

const char * ConsumeWinPath( LPCWSTR lpFileName )
{
	FIXME("add conversion");
	return "";
}

