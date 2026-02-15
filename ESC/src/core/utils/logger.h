#ifndef ES_LOGGER_H
#define ES_LOGGER_H

#include <stdio.h>
#include <stdarg.h>

int es_printf(const char* format, ...);
int es_print_format(const char* format, ...);
int es_fprintf(FILE* stream, const char* format, ...);

#endif