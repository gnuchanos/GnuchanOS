/*
 * wm_png.h — a PNG file, decoded to plain pixels.
 *
 * PNG is the format the settings script's picture is in — `gcl_BAR.call(
 * BackgroundImage="BG.png")` — and reading it means reading zlib, because PNG
 * is zlib with a header and a filter per row. The window manager links no
 * image library: a bar background is not worth pulling one in, so the format
 * is decoded here.
 *
 * The output is one 32-bit card per pixel with alpha in the top byte, which is
 * the same layout a client hands over in _NET_WM_ICON. The window manager
 * therefore has one idea of what an image is, and the drawing code has one
 * thing to understand rather than two.
 */
#ifndef GNUCHANWM_PNG_H
#define GNUCHANWM_PNG_H

/*
 * Decode the PNG at `path`.
 *
 * Returns 0 on success, sets *out_pixels to a malloc'd array of width*height
 * cards that the caller frees, and writes the size to *out_width and
 * *out_height. Returns -1 when the file is missing, is not a PNG, or uses a
 * form of PNG this build does not read — a bit depth other than 8, or Adam7
 * interlacing. Refusing those is deliberate: a picture drawn from a
 * half-understood file is worse than one not drawn at all.
 */
int wm_png_load(const char *path, unsigned int **out_pixels,
                int *out_width, int *out_height);

#endif /* GNUCHANWM_PNG_H */
