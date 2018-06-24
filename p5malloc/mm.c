/*
 * mm.c
 *
 * This is the only file you should modify.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
//#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

//#define DEBUG_VB
#ifdef DEBUG_VB
# define dbg_vb_printf(...) printf(__VA_ARGS__)
#else
# define dbg_vb_printf(...)
#endif

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)


/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       4       /* word size (bytes) */  
#define DSIZE       8       /* doubleword size (bytes) */
#define CHUNKSIZE  (1<<9)  /* initial heap size (bytes) */
#define OVERHEAD    8       /* overhead of header and footer (bytes) */
#define MINPAYLOAD  16    /* payload (prev and next of type void*) (bytes) */
#define ARRAYSIZE (0x58)  /* array of class size at start of heap */

#define MAX(x, y) ((x) > (y)? (x) : (y))  

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
/* NB: this code calls a 32-bit quantity a word */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))
#define PUT_ADDR(p, val)  (*(unsigned long *)(p) = (unsigned long)(val))
#define GET_ADDR(p)  (*(unsigned long*)(p))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)  
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given block ptr bp, compute pointers to next and previous free blocks */
#define NEXT_PTR(bp)  ((char *)(bp) + DSIZE)
#define PREV_PTR(bp)  ((char *)(bp))

/* Given block ptr bp, compute address of next and previous free blocks */
#define NEXT_FREE(bp)  ((char *)(GET_ADDR(NEXT_PTR(bp))))
#define PREV_FREE(bp)  ((char *)(GET_ADDR(PREV_PTR(bp))))

/* Give the payload size of the block */
#define PAYLOAD_SIZE(bp) ((size_t)(GET_SIZE(((char *)(bp) - WSIZE)) - DSIZE))

/* Get the list of free block from the array of size classes */
#define ROOT_LIST(root) ((char *)(GET_ADDR(root)))

/* $end mallocmacros */

/* Global variables */
static char *heap_listp = 0x0;  /* pointer to first block */  
static const char *saveroot;  /* saved address of first array entry */
/* Get the address of the nth array entry */
#define ARRAY(n) ((char *)(saveroot + (n << 0x3)))

/* function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void *indirection(size_t size);
//
static void printblock(void *bp); 
static void checkblock(void *bp);
static void printlist(void *root);
static int in_heap(const void *p);
static int aligned(const void *p);
/* helpers for doubly linked list operations */
static void dbll_init(void *list_ptr, void *bp);
static void dbll_insert_at_root(void *list_ptr, void *bp);
static void dbll_remove_at_root(void *list_ptr, void *bp);
static void dbll_remove(void *list_ptr, void *bp);


/**********************************************************************/
/**********************************************************************/
/**********************************************************************/

/* 
 * mm_init - Initialize the memory manager 
 */
/* $begin mminit */
int mm_init(void) 
{
  /* create the initial empty heap */
  if ((heap_listp = mem_sbrk(ARRAYSIZE+4*WSIZE)) == NULL)
    return -1;
  saveroot = heap_listp;

  PUT(heap_listp+ARRAYSIZE, 0); // alignment padding, 4 bytes, 9-12
  PUT(heap_listp+ARRAYSIZE+WSIZE, PACK(OVERHEAD, 1)); // prologue header, 4 bytes, 13-16
  PUT(heap_listp+ARRAYSIZE+DSIZE, PACK(OVERHEAD, 1)); // prologue footer, 4 bytes, 17-20
  PUT(heap_listp+ARRAYSIZE+WSIZE+DSIZE, PACK(0, 1)); // epilogue header, 4 bytes, 21-24
  heap_listp += ARRAYSIZE+DSIZE;

  // initializing the array
  PUT_ADDR(saveroot, 0x0); // saveroot at the very start of heap, 8 bytes, 0-8
  PUT_ADDR(saveroot+0x8, 0x0);
  PUT_ADDR(saveroot+0x10, 0x0);
  PUT_ADDR(saveroot+0x18, 0x0);
  PUT_ADDR(saveroot+0x20, 0x0);
  PUT_ADDR(saveroot+0x28, 0x0);
  PUT_ADDR(saveroot+0x30, 0x0);
  PUT_ADDR(saveroot+0x38, 0x0);
  PUT_ADDR(saveroot+0x40, 0x0);
  PUT_ADDR(saveroot+0x48, 0x0);
  PUT_ADDR(saveroot+0x50, 0x0);
  
  if ((extend_heap(CHUNKSIZE/WSIZE)) == NULL)
      return -1;
  return 0;
}
/* $end mminit */

/*
 * malloc - Allocate a block with at least size bytes of payload 
 */
/* $begin mmmalloc */
void *mm_malloc(size_t size)
{
  size_t asize;      /* adjusted block size */
  size_t extendsize; /* amount to extend heap if no fit */
  char *bp;      
  if (heap_listp == 0){
    mm_init();
  }

  /* Ignore spurious requests */
  if (size <= 0)
    return NULL;

  /* Adjust block size to include overhead and alignment reqs. */
  if (size <= MINPAYLOAD)
    asize = MINPAYLOAD + OVERHEAD;
  else
    asize = DSIZE * ((size + (OVERHEAD) + (DSIZE-1)) / DSIZE);

  /* Search the free list for a fit */
  if ((bp = find_fit(asize)) != NULL) {
    place(bp, asize);
    return bp;
  }

  /* No fit found. Get more memory and place the block */
  extendsize = MAX(asize,CHUNKSIZE);
  if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
    return NULL;
  place(bp, asize);

  //mm_checkheap(0);
  return bp;
} 
/* $end mmmalloc */

/* 
 * free - Free a block 
 */
/* $begin mmfree */
void mm_free(void *bp)
{
  if (bp == 0) return;

  size_t size = GET_SIZE(HDRP(bp));
  if (heap_listp == 0) {
    mm_init();
  }

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
  //mm_checkheap(0);
}

/* $end mmfree */

/*
 * realloc - Change the size of the block by mallocing a new block,
 *      copying its data, and freeing the old block.  I'm too lazy
 *      to do better.
 */
void *mm_realloc(void *oldptr, size_t size)
{
  size_t oldsize;
  void *newptr;

  /* If size == 0 then this is just free, and we return NULL. */
  if(size == 0) {
    mm_free(oldptr);
    return 0;
  }

  /* If oldptr is NULL, then this is just malloc. */
  if(oldptr == NULL) {
    return mm_malloc(size);
  }

  newptr = mm_malloc(size);

  /* If realloc() fails the original block is left untouched  */
  if(!newptr) {
    return 0;
  }

  /* Copy the old data. */
  oldsize = GET_SIZE(HDRP(oldptr)); 
  if(size < oldsize) oldsize = size;
  memcpy(newptr, oldptr, oldsize);

  /* Free the old block. */
  mm_free(oldptr);

  return newptr;
}

/*
 * calloc - Allocate the block and set it to zero.
 */
void *mm_calloc (size_t nmemb, size_t size)
{
  size_t bytes = nmemb * size;
  void *newptr;

  newptr = mm_malloc(bytes);
  memset(newptr, 0, bytes);

  return newptr;
}

/**********************************************************************/
/**********************************************************************/
/**********************************************************************/

/* The remaining routines are internal helper routines */

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 */
/* $begin mmextendheap */
static void *extend_heap(size_t words) 
{
  char *bp;
  size_t size;
  void *return_ptr;

  /* Allocate an even number of words to maintain alignment */
  size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
  if ((long)(bp = mem_sbrk(size)) < 0) 
    return NULL;

  /* Initialize free block header/footer and the epilogue header */
  PUT(HDRP(bp), PACK(size, 0));         /* free block header */
  PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */

  /* Coalesce if the previous block was free */
  return_ptr = coalesce(bp);

  //mm_checkheap(0);
  return return_ptr;
}
/* $end mmextendheap */

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
/* $begin mmplace */
/* $begin mmplace-proto */
static void place(void *bp, size_t asize)
  /* $end mmplace-proto */
{
  size_t csize = GET_SIZE(HDRP(bp));  
  char *split_bp; 
  size_t split_size;
  char *list_ptr = indirection(csize);

  PUT(HDRP(bp), PACK(csize, 1));
  PUT(FTRP(bp), PACK(csize, 1));

  if ((csize - asize) >= (MINPAYLOAD + OVERHEAD)) {
      PUT(HDRP(bp), PACK(asize, 1));
      PUT(FTRP(bp), PACK(asize, 1));

      dbll_remove(list_ptr, bp);
      split_bp = NEXT_BLKP(bp);
      split_size = csize-asize;
      PUT(HDRP(split_bp), PACK(split_size, 0));
      PUT(FTRP(split_bp), PACK(split_size, 0));
      // find new list for the split block
      list_ptr = indirection(split_size);
      dbll_insert_at_root(list_ptr, split_bp);
  }
  else {
      PUT(HDRP(bp), PACK(csize, 1));
      PUT(HDRP(bp), PACK(csize, 1));

      dbll_remove(list_ptr, bp);
  }
}
/* $end mmplace */

/* 
 * find_fit - Find a fit for a block with asize bytes 
 */
static void *find_fit(size_t asize)
{
  void *bp = NULL;
  char *check_singleton;
  char *start_list_ptr = indirection(asize);
  char *list_ptr, *list;
  char *end_list_ptr = saveroot+0x50;

  for (list_ptr = start_list_ptr; list_ptr <= end_list_ptr; list_ptr += 0x8) {
    list = ROOT_LIST(list_ptr);

    if (list != NULL) {
      // first fit search
      // singleton
      if ((check_singleton = NEXT_FREE(list)) == NULL) {
        if (!GET_ALLOC(HDRP(list)) && (asize <= GET_SIZE(HDRP(list)))) {
        bp = list;
        return bp;
        }
      }
      // search
      for (bp = list; bp != NULL; bp = NEXT_FREE(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
          return bp;
        }
      }
    }
    // else continue onto other lists
  }
  return NULL; // not found
}

/*
 * coalesce - boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp) 
{
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));
  size_t prev_size, next_size;
  char *list_ptr = indirection(size);
  char *list = ROOT_LIST(list_ptr);
  char *prev, *next, *list_ptr_prev, *list_ptr_next;

  /* initiate list on first call to extend_heap */
  if (list == 0x0) {
    dbll_init(list_ptr, bp);
    return bp;
  }

  /* heap_extend, Case 3; free, any Case */
  if (prev_alloc && next_alloc) {            /* Case 1 */
    dbll_insert_at_root(list_ptr, bp);
    return bp;
  }

  else if (prev_alloc && !next_alloc) {      /* Case 2 */
    next = NEXT_BLKP(bp);
    assert (next != NULL);
    next_size = GET_SIZE(HDRP(next));
    list_ptr_next = indirection(next_size);
    dbll_remove(list_ptr_next, next);

    size += next_size;
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size,0));

    list_ptr = indirection(size);
    dbll_insert_at_root(list_ptr, bp);
  }

  else if (!prev_alloc && next_alloc) {      /* Case 3 */
    prev = PREV_BLKP(bp);
    assert (prev != NULL);
    prev_size = GET_SIZE(HDRP(prev));
    list_ptr_prev = indirection(prev_size);
    dbll_remove(list_ptr_prev, prev);

    size += prev_size;
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(prev), PACK(size, 0));
    bp = prev;

    list_ptr = indirection(size);
    dbll_insert_at_root(list_ptr, bp);
  }

  else {                                     /* Case 4 */
    prev = PREV_BLKP(bp);
    next = NEXT_BLKP(bp);
    assert (prev != NULL && next != NULL);

    prev_size = GET_SIZE(HDRP(prev));
    list_ptr_prev = indirection(prev_size);
    dbll_remove(list_ptr_prev, prev);

    next_size = GET_SIZE(HDRP(next));
    list_ptr_next = indirection(next_size);
    dbll_remove(list_ptr_next, next);

    size += prev_size + next_size;
    PUT(HDRP(prev), PACK(size, 0));
    PUT(FTRP(next), PACK(size, 0));
    bp = prev;

    list_ptr = indirection(size);
    dbll_insert_at_root(list_ptr, bp);
  }

  return bp;
}

static void *indirection(size_t size)
{
  assert (size >= 0);
  if (size < (1<<5))
    return ((char *)saveroot);
  else if (size < (1<<6))
    return ((char *)saveroot+0x8);
  else if (size < (1<<7))
    return ((char *)saveroot+0x10);
  else if (size < (1<<8))
    return ((char *)saveroot+0x18);
  else if (size < (1<<9))
    return ((char *)saveroot+0x20);
  else if (size < (1<<10))
    return ((char *)saveroot+0x28);
  else if (size < (1<<11))
    return ((char *)saveroot+0x30);
  else if (size < (1<<12))
    return ((char *)saveroot+0x38);
  else if (size < (1<<13))
    return ((char *)saveroot+0x40);
  else if (size < (1<<14))
    return ((char *)saveroot+0x48);
  else
    return ((char *)saveroot+0x50);
}

/**********************************************************************/
// Helpers for doubly linked list
// Returns the starting block of the list

// Initializing the explicit list for the first time
// set root to the chunk created in the first call to expend_heap
static void dbll_init(void *list_ptr, void *bp)
{
  PUT_ADDR(PREV_PTR(bp), 0x0); // bp->prev = NULL
  PUT_ADDR(NEXT_PTR(bp), 0x0); // bp->next = NULL
  PUT_ADDR(list_ptr, bp); // root points to the newest free block
}

// Insert a block to the root of the list
static void dbll_insert_at_root(void *list_ptr, void *bp)
{
  char *root = ROOT_LIST(list_ptr);
  if (bp == NULL) {
    printf("Error: given NULL bp in dbll_insert_at_root\n");
    return NULL;
  }
  if (root == NULL) {
    return dbll_init(list_ptr, bp);
  }

  char *savebp = root; // save the old block root pointed to

  PUT_ADDR(PREV_PTR(savebp), bp); // savebp->prev = bp;
  PUT_ADDR(PREV_PTR(bp), 0x0); // bp->prev = NULL
  PUT_ADDR(NEXT_PTR(bp), savebp); // bp->next = savebp
  PUT_ADDR(list_ptr, bp); // root now points to the newest free block
}
// Remove a block from the root of the list
static void dbll_remove_at_root(void *list_ptr, void *bp)
{
  char *root = ROOT_LIST(list_ptr);
  assert (root == bp);

  char *next = NEXT_FREE(bp); // get the block after bp
  if (next == NULL) {
    PUT_ADDR(list_ptr, 0x0);
    return NULL;
  }
  PUT_ADDR(PREV_PTR(next), 0x0);
  PUT_ADDR(list_ptr, next); // root = bp->next
}
// Remove a block from either the root or the middle of the list
static void dbll_remove(void *list_ptr, void *bp)
{
  char *root = ROOT_LIST(list_ptr);
  char *prev = PREV_FREE(bp); // get prev address
  char *next = NEXT_FREE(bp); // get next address

  assert (root && bp);
  // remove at root
  if (root == bp) {
    assert (PREV_FREE(bp) == NULL);
    // singleton list
    if (next == NULL) {
      PUT_ADDR(list_ptr, 0x0);
      return NULL;
    }
    PUT_ADDR(PREV_PTR(next), 0x0);
    PUT_ADDR(list_ptr, next); // root = bp->next
  }

  if (prev != NULL)
    PUT_ADDR(NEXT_PTR(prev), next); // bp->prev->next = bp->next
  if (next != NULL)
    PUT_ADDR(PREV_PTR(next), prev); // bp->next->prev = bp->prev
  // don't move the root!
}

/**********************************************************************/
// Printing and checking helpers for debugging

/* 
 * checkheap - Minimal check of the heap for consistency 
 */
void mm_checkheap(int verbose)
{
  char *bp = heap_listp;

  if (verbose)
    printf("\nHeap (%p):\n", heap_listp);

  if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
    printf("Bad prologue header\n");
  checkblock(heap_listp);

  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    if (verbose)  {
      printblock(bp);
    }
    checkblock(bp);
  }

  if (verbose) {
    printblock(bp);
  }
  if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
    printf("Bad epilogue header\n");
  
  if (verbose) {
    char *root = ROOT_LIST(saveroot);
    printlist(root);
  }
}

static void printblock(void *bp) 
{
  size_t hsize, halloc, fsize, falloc;

  hsize = GET_SIZE(HDRP(bp));
  halloc = GET_ALLOC(HDRP(bp));  
  fsize = GET_SIZE(FTRP(bp));
  falloc = GET_ALLOC(FTRP(bp));  

  if (hsize == 0) {
    printf("%p: EOL\n", bp);
    return;
  }

  printf("%p: header: [%p:%c] footer: [%p:%c]\n", bp, 
    hsize, (halloc ? 'a' : 'f'), 
    fsize, (falloc ? 'a' : 'f')); 
  // if (halloc == 0) {
  //   char *prev = PREV_FREE(bp);
  //   char *next = NEXT_FREE(bp);
  //   printf("          prev: [%p] next: [%p]\n", prev, next);
  // }
}

static void checkblock(void *bp) 
{
  if ((size_t)bp % 8)
    printf("Error: %p is not doubleword aligned\n", bp);
  if (GET(HDRP(bp)) != GET(FTRP(bp)))
    printf("Error: header does not match footer\n");
  // check prev and next for free block
  if (!GET_ALLOC(HDRP(bp))) {
    char *prev = PREV_FREE(bp);
    char *next = NEXT_FREE(bp);
    if ((prev != NULL) && (next != NULL)) {
      assert (prev != next);
      assert (prev != bp);
      assert (next != bp);
      assert (in_heap(prev));
      assert (in_heap(next));
    }
  }
}

static void printlist(void *root)
{
  char *bp = root;
  char *next_of_prev = NULL;
  char *prev_of_next = NULL;
  printf("\nRoot (%p):\n", root);
  if (root == NULL)
    return;
  if ((NEXT_FREE(bp)) == NULL) {
    //printf("          bp: [%p]\n", bp);
    printblock(bp);
  }
  for ( ; (NEXT_FREE(bp)) != NULL; bp = (NEXT_FREE(bp))) {
    if (GET_ALLOC(HDRP(bp)))
      printf("Error: block [%p] is not free\n", bp);
    char *prev = PREV_FREE(bp);
    char *next = NEXT_FREE(bp);
    if (prev != NULL)
      next_of_prev = NEXT_FREE(prev);
    if (next != NULL)
      prev_of_next = PREV_FREE(next);
    if ((next_of_prev != NULL && prev_of_next != NULL) &&
      (next_of_prev != bp || prev_of_next != bp))
      printf("Error:    bp: [%p] prev: [%p] next: [%p]\n", bp, prev, next);
    //printf("          bp: [%p]\n", bp);
    printblock(bp);
  }
  if ((NEXT_FREE(bp)) == NULL && bp != root)
    //printf("          bp: [%p]\n", bp);
    printblock(bp);
    printf("          EOL\n");
}

/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p) {
    return p < mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}
