/*
 * image_decode.h - JPEG and PNG decoding, by way of the vendored stb_image
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_CORE_IMAGE_DECODE_H
#define PSYMP3_CORE_IMAGE_DECODE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The largest width or height psymp3_image_decode_rgba() will decode. An
 * image is held as 4 bytes a pixel, so this bounds one decode at 256 MiB. */
#define PSYMP3_IMAGE_MAX_DIMENSION 8192

/**
 * Decode a JPEG or PNG held in memory.
 *
 * @return width * height pixels of 8-bit R, G, B, A (alpha not premultiplied),
 *         top row first, to be released with psymp3_image_free(); or NULL if
 *         the data is neither format, is damaged, is larger than
 *         PSYMP3_IMAGE_MAX_DIMENSION either way, or memory ran out.
 */
unsigned char *psymp3_image_decode_rgba(const unsigned char *data, size_t size,
                                        int *width, int *height);

void psymp3_image_free(unsigned char *pixels);

#ifdef __cplusplus
}
#endif

#endif /* PSYMP3_CORE_IMAGE_DECODE_H */
