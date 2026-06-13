/* $Id$ */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define DIRMAXLEN 1024

char *dirname(const char *path) {
  size_t l = 0;
  static char dir[DIRMAXLEN];
  dir[0] = 0;
#ifdef __WIN32__
  for(l = strlen(path); l > 0 && path[l] != '\\'; l--);
#else
  for(l = strlen(path); l > 0 && path[l] != '/'; l--);
#endif
  if(l >= DIRMAXLEN) {
    errno = ENAMETOOLONG;
    return NULL;
  }
  if(l == 0)
    return ".";
  strncpy(dir, path, l);
  dir[l] = '\0';
  return dir;
}
