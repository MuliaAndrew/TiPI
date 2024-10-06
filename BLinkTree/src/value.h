#ifndef VALUE_H
#define VALUE_H

#include "string.h"

struct __attribute__((__packed__)) Value {
    char data[16];
};

typedef struct Value Value;

#endif // VALUE_H