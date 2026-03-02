#ifndef MPG123_H
#define MPG123_H

#include <sys/types.h>
#include <stddef.h>
#include <stdio.h> // for SEEK_SET if needed

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#define MPG123_OK 0
#define MPG123_DONE -12
#define MPG123_ERR -1
#define MPG123_ADD_FLAGS 1
#define MPG123_QUIET 2
#define MPG123_ENC_SIGNED_16 16

struct mpg123_handle_struct;
typedef struct mpg123_handle_struct mpg123_handle;

mpg123_handle *mpg123_new(const char *decoder, int *error);
void mpg123_delete(mpg123_handle *mh);
int mpg123_param(mpg123_handle *mh, long type, long value, double fvalue);
int mpg123_open_handle(mpg123_handle *mh, void *iohandle);
int mpg123_replace_reader_handle(mpg123_handle *mh, ssize_t (*r_read) (void *, void *, size_t), off_t (*r_lseek) (void *, off_t, int), void (*cleanup) (void *));
int mpg123_getformat(mpg123_handle *mh, long *rate, int *channels, int *encoding);
int mpg123_format_none(mpg123_handle *mh);
int mpg123_format(mpg123_handle *mh, long rate, int channels, int encodings);
int mpg123_close(mpg123_handle *mh);
off_t mpg123_length(mpg123_handle *mh);
off_t mpg123_tell(mpg123_handle *mh);
int mpg123_read(mpg123_handle *mh, unsigned char *outmemory, size_t outmemsize, size_t *done);
off_t mpg123_seek(mpg123_handle *mh, off_t sampleoff, int whence);
const char* mpg123_plain_strerror(int errcode);

#endif
