// SPDX-License-Identifier: CC0-1.0
//
// SPDX-FileContributor: Antonio Niño Díaz, 2022-2026

#include <stdint.h>
#include <errno.h>

// This file implements stubs for system calls.

int read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;

    return len;
}

int write(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;

    return len;
}

int close(int file)
{
    (void)file;

    return -1;
}

int lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;

    return 0;
}

int open(char *path, int flags, ...)
{
    (void)path;
    (void)flags;

    return -1;
}

void *sbrk(int incr)
{
    // Symbols defined by the linker
    extern char __HEAP_START__[];
    extern char __HEAP_END__[];
    const uintptr_t HEAP_START = (uintptr_t) __HEAP_START__;
    const uintptr_t HEAP_END = (uintptr_t) __HEAP_END__;

    // Pointer to the current end of the heap
    static uintptr_t heap_end = HEAP_START;

    if (heap_end + incr > HEAP_END)
    {
        errno = ENOMEM;
        return (void *)-1;
    }

    uintptr_t prev_heap_end = heap_end;

    heap_end += incr;

    return (void *)prev_heap_end;
}
