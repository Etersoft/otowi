#include <windows.h>
#include <stdio.h>

int main()
{
    const char name[] = "test.txt";
    HANDLE hFile;

    otowi_init();

    printf("Try to open file \n", name);
    hFile = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
	printf("Error! Status: %d\n", GetLastError());
    else
    {
	printf("Success. Press enter to quit.\n");
	getchar();
    }
    return 0;
}
