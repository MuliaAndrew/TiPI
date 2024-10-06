#ifndef VALUE_H
#define VALUE_H

#include "string.h"

struct __attribute__((__packed__)) Value {
    size_t size;
    char* data;
};

typedef struct Value Value;

#endif // VALUE_H