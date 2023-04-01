#ifndef KMALLOC_H
#define KMALLOC_H
#include "kmalloc.h"
#include <stdlib.h>

void *kmalloc(size_t size)
{
    return malloc(size);
}

#endif // KMALLOC_H
