#ifndef GCL_IO_H
#define GCL_IO_H

/* Read entire file into heap-allocated buffer; returns NULL on error. Caller must free. */
char *file_read(const char *path);

/* Copy a source file to a destination directory; returns 1 on success, 0 on error */
int file_copy_to_dir(const char *src, const char *dst_dir);

#endif
