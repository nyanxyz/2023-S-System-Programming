/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */
#define CHUNKSIZE (1 << 12) /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static void *extend_heap(size_t words);
static void *find_fit(size_t size);
static void place(void *bp, size_t size);
static void *coalesce(void *bp);

int mm_check(void);

/* Global variables */
static char *heap_listp = NULL;

/* place - Place the requested block and remove it from the free list */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        return -1;
    }
    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2 * WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }

    mm_check();

    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) {
        return NULL;
    }

    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);

    mm_check();
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);

    mm_check();
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    // If ptr is NULL, the call is equivalent to mm_malloc(size)
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    // If size is 0, the call is equivalent to mm_free(ptr)
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size_t asize = MAX(ALIGN(size) + DSIZE, 2 * DSIZE);
    size_t csize = GET_SIZE(HDRP(ptr));

    // If the new size is equal to the old size, do nothing and return the original block pointer
    if (asize == csize) {
        return ptr;
    }

    // If the new size is smaller than the old size, shrink the block and place the remaining free block in the free list
    if (asize < csize) {
        size_t new_size = csize - asize;
        if (new_size >= asize + 2 * DSIZE) {
            PUT(HDRP(ptr), PACK(asize, 1));
            PUT(FTRP(ptr), PACK(asize, 1));
            void *bp = NEXT_BLKP(ptr);
            PUT(HDRP(bp), PACK(new_size, 0));
            PUT(FTRP(bp), PACK(new_size, 0));
            coalesce(bp);
        }
        return ptr;
    }

    // If the new size is larger than the old size, allocate a new block and copy the old data to the new block
    void *new_ptr = mm_malloc(size);

    if (new_ptr == NULL) {
        return NULL;
    }

    memcpy(new_ptr, ptr, csize - DSIZE);
    mm_free(ptr);

    mm_check();
    return new_ptr;
}

// words만큼 힙을 확장하고, 확장한 힙의 시작 주소를 반환한다.
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    // Initialize the new free block header/footer and the new epilogue header
    PUT(HDRP(bp), PACK(size, 0));         // Free block header
    PUT(FTRP(bp), PACK(size, 0));         // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // New epilogue header

    // Coalesce if the previous block was free
    return coalesce(bp);
}

// Coalesce adjacent free blocks
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) { // Case 1: Both previous and next blocks are allocated
        return bp;
    }
    
    else if (prev_alloc && !next_alloc) { // Case 2: Previous block is allocated, next block is free
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    
    else if (!prev_alloc && next_alloc) { // Case 3: Previous block is free, next block is allocated
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    
    else { // Case 4: Both previous and next blocks are free
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}

// Find an appropriate free block in the free list
static void *find_fit(size_t size)
{
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (size <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }

    return NULL;
}

int mm_check(void)
{
    void *bp;
    int heap_ok = 1;

    // Check prologue and epilogue blocks
    if ((GET_SIZE(heap_listp) != DSIZE) || !GET_ALLOC(heap_listp)) {
        printf("Error: Bad prologue header\n");
        heap_ok = 0;
    }
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (GET_ALLOC(HDRP(bp)) != GET_ALLOC(FTRP(bp))) {
            printf("Error: Header and footer allocation bits do not match\n");
            heap_ok = 0;
        }
    }
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp)))) {
        printf("Error: Bad epilogue header\n");
        heap_ok = 0;
    }

    // Check free list consistency
    // for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    //     if (!GET_ALLOC(HDRP(bp))) {
    //         if (bp != find_fit(GET_SIZE(HDRP(bp)))) {
    //             printf("Error: Free block not in free list\n");
    //             heap_ok = 0;
    //         }
    //         if (bp != heap_listp && GET_ALLOC(HDRP(PREV_BLKP(bp)))) {
    //             printf("Error: Free block's predecessor is not free\n");
    //             heap_ok = 0;
    //         }
    //         if (bp != NEXT_BLKP(heap_listp) && GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
    //             printf("Error: Free block's successor is not free\n");
    //             heap_ok = 0;
    //         }
    //     }
    // } 

    // Check for contiguous free blocks that escaped coalescing
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && !GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
            printf("Error: Contiguous free blocks escaped coalescing\n");
            heap_ok = 0;
        }
    }

    return heap_ok;
}