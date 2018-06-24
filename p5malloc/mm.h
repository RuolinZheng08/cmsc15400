#include <stdio.h>

extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);
extern void *mm_calloc (size_t nmemb, size_t size);
extern int mm_init(void);

/* This is largely for debugging.  You can do what you want with the
   verbose flag; we don't care. */
extern void mm_checkheap(int verbose);
