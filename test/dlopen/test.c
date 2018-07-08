#include <dlfcn.h>
#include <stdio.h>

int main()
{
    void *handle = dlopen(0, RTLD_LAZY);
    if (!handle)
        printf ("dlopen error: %s\n", dlerror());
    void *r = dlsym(handle, "printf");
    printf ("handle = %p, func = %p, &printf = %p\n", handle, r, printf);
}

