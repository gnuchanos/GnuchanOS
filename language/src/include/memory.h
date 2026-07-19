#ifndef GCL_MEMORY_H
#define GCL_MEMORY_H

#include "gcl.h"

/* arena: gecici tahsisler, reset ile toptan serbest */
typedef struct Arena {
    char *ptr, *end;
    struct Arena *next;
} Arena;

void  mem_init(void);
void  mem_shutdown(void);
void *area(size_t sz);           /* akilli alloc: pool/freelist/page */
void  area_free(void *p);        
void *arena_alloc(size_t sz);    /* hizli tahsis */
void  arena_reset(void);         /* tum arenayi bosalt */

#endif
