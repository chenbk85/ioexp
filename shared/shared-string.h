// vim: set ts=2 sw=2 tw=99 et:
// 
// Copyright (C) 2014 David Anderson
// 
// This file is part of the AlliedModders I/O Library.
// 
// The AlliedModders I/O library is licensed under the GNU General Public
// License, version 3 or higher. For more information, see LICENSE.txt
//
#ifndef _include_amio_shared_strfmt_h_
#define _include_amio_shared_strfmt_h_

#include <string.h>
#include <stdarg.h>

namespace amio {

// Both of these format a string and return a buffer allocated with new[].
char *FormatStringVa(const char *fmt, va_list ap);
char *FormatString(const char *fmt, ...);
size_t FormatArgs(char *buffer, size_t maxlength, const char *fmt, ...);
size_t FormatArgsVa(char *buffer, size_t maxlength, const char *fmt, va_list ap);

static inline size_t
CopyString(char *dest, size_t maxlength, const char *src)
{
  if (!maxlength)
    return 0;
  char *start = dest;
  while (*src && --maxlength)
    *dest++ = *src++;
  *dest = '\0';
  return dest - start;
}

}

#endif // _include_amio_shared_strfmt_h_
