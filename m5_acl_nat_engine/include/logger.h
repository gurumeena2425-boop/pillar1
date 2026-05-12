#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

FILE *open_log_file(const char *filename);
void close_log_file(FILE *fp);

#endif
