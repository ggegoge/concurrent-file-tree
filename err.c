#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include "err.h"

void syserr(int e, const char* fmt, ...)
{
  va_list fmt_args;

  if (!e)
    return;

  fprintf(stderr, "ERROR: ");

  va_start(fmt_args, fmt);
  vfprintf(stderr, fmt, fmt_args);
  va_end (fmt_args);
  fprintf(stderr, " (%d; %s)\n", e, strerror(e));
  exit(1);
}
