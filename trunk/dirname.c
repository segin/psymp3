#include <stdio.h>
#include <string.h>
#include <errno.h>

#define DIRMAXLEN 1024

char *dirname(const char *path) {
  int l = 0;
  static char dir[DIRMAXLEN];
  dir[0] = 0;
#ifdef __WIN32__
  for(l = strlen(path); path[l] != '\\' && l > 0; l--);
#else
  for(l = strlen(path); path[l] != '/' && l > 0; l--);
#endif
  if(l >= DIRMAXLEN) {
    errno = ENAMETOOLONG;
    return NULL;
  }
  if(l == 0)
    return ".";
  strncpy(dir, path, l);
  return dir;
}

