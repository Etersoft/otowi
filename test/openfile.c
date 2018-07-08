#include <windows.h>
#include <stdio.h>

int main()
{
    const char name[] = "/etc/fstab";
    HANDLE hFile;

    otowi_init();

    char *buffer = (char*)HeapAlloc( GetProcessHeap(), 0, 100 );
    printf("%p\n", buffer);
    sprintf(buffer, "buffer address: %p\n", buffer);
    printf("%s\n", buffer);
    HeapFree(GetProcessHeap(), 0, (PVOID)buffer);

    printf("Try to open file \n", name);
    hFile = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
	printf("Error! Status: %d\n", GetLastError());
    else
    {
	printf("Success. Press enter to quit.\n");
	getchar();
    }
    return 0;
}
