/*
 * wm_png.c — decode a PNG file into plain pixels.
 *
 * PNG is a signature, a run of chunks, and a zlib stream. This reads the three
 * chunks a picture needs — IHDR for the size and the colour form, PLTE for a
 * palette, IDAT for the pixels — and inflates the pixels with zlib, which the
 * system already has. A format that needs an image library to read a bar
 * background would be a strange reason to add a dependency, so it is read
 * here.
 *
 * What is not read is refused rather than guessed at: a 16-bit image, a
 * palette of fewer bits, Adam7 interlacing. Each of those would draw as
 * something wrong if half-understood, and a bar with a wrong picture is worse
 * than a bar that fell back to its colour.
 *
 * The result is one card per pixel, R in bits 16-23, G in 8-15, B in 0-7 and
 * A in 24-31 — the layout _NET_WM_ICON uses, so the rest of the window manager
 * has one idea of what an image is.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include "wm_png.h"

/* The eight bytes every PNG begins with. A file that does not start with them
   is not a PNG whatever else it holds. */
static const unsigned char PNG_SIGNATURE[8] = {
    0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a,
};

/* A PNG chunk's length is a 32-bit big-endian number, and so is every number
   in a PNG header; the format is network byte order throughout. */
static unsigned int read_u32(const unsigned char *bytes) {
    return ((unsigned int)bytes[0] << 24) |
           ((unsigned int)bytes[1] << 16) |
           ((unsigned int)bytes[2] << 8) |
           ((unsigned int)bytes[3]);
}

/* --- reading the whole file ----------------------------------------------- */

/* The file, whole, in memory. A PNG has to be walked chunk by chunk and the
   pixels come out of a compressed stream, so there is nothing to be gained by
   streaming it — and a bar background is small. */
static unsigned char *read_file(const char *path, long *size_out) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long size = ftell(file);
    if (size <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    unsigned char *data = malloc((size_t)size);
    if (!data) {
        fclose(file);
        return NULL;
    }
    if (fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *size_out = size;
    return data;
}

/* --- unfiltering ----------------------------------------------------------
 *
 * Before compression, PNG filters every row against the row above and the
 * pixel to the left, so that a flat picture compresses to almost nothing. The
 * filters are undone here, per row, before the pixels mean anything.
 *
 * The first byte of every row names its filter, and the bytes after it are
 * that row. Five filters exist; each one is a different sum of the pixel to
 * the left, the one above, and the one above-left, so all five are handled
 * here and a sixth value is refused.
 */

static unsigned char paeth(unsigned char left, unsigned char above,
                           unsigned char above_left) {
    int estimate = (int)left + (int)above - (int)above_left;
    int to_left = estimate - (int)left;
    int to_above = estimate - (int)above;
    int to_corner = estimate - (int)above_left;
    if (to_left < 0) to_left = -to_left;
    if (to_above < 0) to_above = -to_above;
    if (to_corner < 0) to_corner = -to_corner;
    if (to_left <= to_above && to_left <= to_corner) {
        return left;
    }
    if (to_above <= to_corner) {
        return above;
    }
    return above_left;
}

/* Undo the filtering over the whole image, in place: each row is made absolute
   from the row above it, which is already absolute because rows are walked top
   to bottom. */
static int unfilter(unsigned char *raw, int height, int stride) {
    for (int y = 0; y < height; y++) {
        unsigned char filter = raw[y * (stride + 1)];
        unsigned char *row = raw + y * (stride + 1) + 1;
        unsigned char *above = y > 0 ? row - (stride + 1) : NULL;

        for (int x = 0; x < stride; x++) {
            unsigned char left = x >= 1 ? row[x - 1] : 0;
            unsigned char up = above ? above[x] : 0;
            unsigned char up_left = (above && x >= 1) ? above[x - 1] : 0;
            switch (filter) {
            case 0:
                break;
            case 1:
                row[x] = (unsigned char)(row[x] + left);
                break;
            case 2:
                row[x] = (unsigned char)(row[x] + up);
                break;
            case 3:
                row[x] = (unsigned char)(row[x] + ((left + up) >> 1));
                break;
            case 4:
                row[x] = (unsigned char)(row[x] + paeth(left, up, up_left));
                break;
            default:
                return -1;
            }
        }
    }
    return 0;
}

/* --- chunks --------------------------------------------------------------- */

/* Where a named chunk's data begins, or -1. Chunks are walked from the start
   of the file, each one a length, a name, the data and a CRC. */
static long find_chunk(const unsigned char *data, long size,
                       const char name[4]) {
    long at = 8;   /* past the signature */
    while (at + 8 <= size) {
        unsigned int length = read_u32(data + at);
        const unsigned char *type = data + at + 4;
        if (at + 8 + (long)length > size) {
            return -1;
        }
        if (memcmp(type, name, 4) == 0) {
            return at + 8;
        }
        at += 8 + (long)length + 4;   /* data and CRC */
    }
    return -1;
}

/* --- decoding ------------------------------------------------------------- */

int wm_png_load(const char *path, unsigned int **out_pixels,
                int *out_width, int *out_height) {
    if (!path || !path[0] || !out_pixels || !out_width || !out_height) {
        return -1;
    }

    long size = 0;
    unsigned char *file = read_file(path, &size);
    if (!file || size < 8 || memcmp(file, PNG_SIGNATURE, 8) != 0) {
        free(file);
        return -1;
    }

    /* IHDR: width, height, bit depth, colour type, compression, filter,
       interlace. It is the first chunk always, so it is read from a fixed
       offset rather than searched for. */
    if (size < 8 + 25 || memcmp(file + 12, "IHDR", 4) != 0) {
        free(file);
        return -1;
    }
    int width = (int)read_u32(file + 16);
    int height = (int)read_u32(file + 20);
    int depth = file[24];
    int colour_type = file[25];
    int interlace = file[28];

    /* Only the forms a bar background is written in, and only 8 bits deep.
       Everything else is refused rather than guessed at. */
    if (width <= 0 || height <= 0 || depth != 8 || interlace != 0) {
        free(file);
        return -1;
    }

    int channels = 0;
    switch (colour_type) {
    case 0: channels = 1; break;   /* grey           */
    case 2: channels = 3; break;   /* truecolour     */
    case 3: channels = 1; break;   /* palette        */
    case 4: channels = 2; break;   /* grey + alpha   */
    case 6: channels = 4; break;   /* truecolour + a */
    default:
        free(file);
        return -1;
    }

    /* A palette is read whole before the pixels, because a pixel is an index
       into it and an index needs the table to mean anything. */
    unsigned char palette[256 * 3];
    memset(palette, 0, sizeof(palette));
    int palette_count = 0;
    if (colour_type == 3) {
        long at = find_chunk(file, size, "PLTE");
        if (at < 0) {
            free(file);
            return -1;
        }
        unsigned int length = read_u32(file + at - 8);
        palette_count = (int)(length / 3);
        if (palette_count > 256) {
            palette_count = 256;
        }
        memcpy(palette, file + at, (size_t)palette_count * 3);
    }

    /* Every IDAT chunk is one piece of the same zlib stream, so they are
       copied out one after another, in file order, into a buffer the
       decompressor reads. */
    long idat_start = find_chunk(file, size, "IDAT");
    if (idat_start < 0) {
        free(file);
        return -1;
    }

    long stream_size = 0;
    {
        long at = idat_start - 8;   /* back to the chunk's length */
        while (at + 8 <= size) {
            unsigned int length = read_u32(file + at);
            if (at + 8 + (long)length > size) {
                break;
            }
            if (memcmp(file + at + 4, "IDAT", 4) != 0) {
                break;
            }
            stream_size += (long)length;
            at += 8 + (long)length + 4;
        }
    }

    unsigned char *stream = malloc((size_t)stream_size);
    if (!stream) {
        free(file);
        return -1;
    }
    {
        long written = 0;
        long at = idat_start - 8;
        while (at + 8 <= size && written < stream_size) {
            unsigned int length = read_u32(file + at);
            if (memcmp(file + at + 4, "IDAT", 4) != 0) {
                break;
            }
            memcpy(stream + written, file + at + 8, (size_t)length);
            written += (long)length;
            at += 8 + (long)length + 4;
        }
    }

    /* The pixels come out as one filter byte and one row of samples per line.
       What that line is worth in bytes is fixed by the colour form, which is
       why the size is known before anything is decompressed. */
    int stride = width * channels;
    long expected = (long)height * (long)(stride + 1);
    unsigned char *raw = malloc((size_t)expected);
    if (!raw) {
        free(stream);
        free(file);
        return -1;
    }

    uLongf produced = (uLongf)expected;
    int status = uncompress(raw, &produced, stream, (uLong)stream_size);
    free(stream);
    free(file);
    if (status != Z_OK || (long)produced != expected) {
        free(raw);
        return -1;
    }

    if (unfilter(raw, height, stride) != 0) {
        free(raw);
        return -1;
    }

    /* One card per pixel, in the layout the rest of the window manager reads:
       R, G, B below the alpha byte. A palette entry and a grey sample both
       become a full colour here, so the drawing code never has to know which
       form it came from. */
    unsigned int *pixels = malloc((size_t)width * (size_t)height *
                                  sizeof(unsigned int));
    if (!pixels) {
        free(raw);
        return -1;
    }

    for (int y = 0; y < height; y++) {
        const unsigned char *row = raw + (long)y * (stride + 1) + 1;
        for (int x = 0; x < width; x++) {
            unsigned int red, green, blue, alpha = 0xff;
            const unsigned char *sample = row + x * channels;
            switch (colour_type) {
            case 0:
                red = green = blue = sample[0];
                break;
            case 2:
                red = sample[0];
                green = sample[1];
                blue = sample[2];
                break;
            case 3: {
                int index = sample[0];
                if (index >= palette_count) {
                    index = 0;
                }
                red = palette[index * 3];
                green = palette[index * 3 + 1];
                blue = palette[index * 3 + 2];
                break;
            }
            case 4:
                red = green = blue = sample[0];
                alpha = sample[1];
                break;
            default:   /* 6 */
                red = sample[0];
                green = sample[1];
                blue = sample[2];
                alpha = sample[3];
                break;
            }
            pixels[(long)y * width + x] =
                (alpha << 24) | (red << 16) | (green << 8) | blue;
        }
    }

    free(raw);
    *out_pixels = pixels;
    *out_width = width;
    *out_height = height;
    return 0;
}
