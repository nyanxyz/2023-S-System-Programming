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

/* Global variables */
static char *heap_listp = NULL;

// Extend the heap with a new free block
static void *extend_heap(size_t size)
{
    char *bp;

    // Align the requested size
    size = ALIGN(size);

    // Request more memory from the system
    if ((bp = mem_sbrk(size)) == (void *)-1) {
        return NULL;
    }

    // Initialize the new free block header/footer and the new epilogue header
    PUT(HDRP(bp), PACK(size, 0));         // Free block header
    PUT(FTRP(bp), PACK(size, 0));         // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // New epilogue header

    // Coalesce if the previous block was free
    return coalesce(bp);
}

// Find an appropriate free block in the free list
static void *find_fit(size_t size)
{
    // Start searching from the beginning of the free list
    void *bp = heap_listp;

    // Iterate through the blocks until the end of the list
    while (GET_SIZE(HDRP(bp)) > 0) {
        // Check if the block is not allocated and is large enough to fit the requested size
        if (!GET_ALLOC(HDRP(bp)) && (size <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
        bp = NEXT_BLKP(bp);
    }

    // If no suitable block was found, return NULL
    return NULL;
}

/* place - Place the requested block and remove it from the free list */
static void place(void *bp, size_t size)
{
    size_t asize = GET_SIZE(HDRP(bp));
    if ((asize - size) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(asize - size, 0));
        PUT(FTRP(bp), PACK(asize - size, 0));
    } else {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
    }
}

// Coalesce adjacent free blocks
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) { // Case 1: Both previous and next blocks are allocated
        return bp;
    } else if (prev_alloc && !next_alloc) { // Case 2: Previous block is allocated, next block is free
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) { // Case 3: Previous block is free, next block is allocated
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else { // Case 4: Both previous and next blocks are free
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) - 1) {
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

    if (size == 0)
        return NULL;

    asize = MAX(ALIGN(size) + DSIZE, 2 * DSIZE);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if (ptr == NULL)
        return;

    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        // If ptr is NULL, mm_realloc should act like mm_malloc.
        return mm_malloc(size);
    }

    if (size == 0) {
        // If size is zero, mm_realloc should act like mm_free.
        mm_free(ptr);
        return NULL;
    }

    // Calculate the new size with alignment
    size_t new_size = ALIGN(size + SIZE_T_SIZE);
    size_t old_size = GET_SIZE(HDRP(ptr));

    if (new_size == old_size) {
        // If the new size is equal to the old size, return the original pointer.
        return ptr;
    }

    if (new_size < old_size) {
        // If the new size is smaller than the old size, shrink the block.
        size_t remaining_size = old_size - new_size;
        if (remaining_size >= 2 * DSIZE) {
            PUT(HDRP(ptr), PACK(new_size, 1));
            PUT(FTRP(ptr), PACK(new_size, 1));
            PUT(HDRP(NEXT_BLKP(ptr)), PACK(remaining_size, 0));
            PUT(FTRP(NEXT_BLKP(ptr)), PACK(remaining_size, 0));
            mm_free(NEXT_BLKP(ptr));
        }
        return ptr;
    }

    // If the new size is larger than the old size, attempt to extend the block.
    size_t total_size = old_size;
    void *next_block = NEXT_BLKP(ptr);
    while (total_size < new_size && !GET_ALLOC(HDRP(next_block))) {
        total_size += GET_SIZE(HDRP(next_block));
        next_block = NEXT_BLKP(next_block);
    }

    if (total_size >= new_size) {
        mm_free(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(total_size, 1));
        PUT(FTRP(ptr), PACK(total_size, 1));
        return ptr;
    }

    // If extension is not possible, allocate a new block.
    void *new_ptr = mm_malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }

    // Copy the data from the old block to the new block.
    memcpy(new_ptr, ptr, old_size - SIZE_T_SIZE);

    // Free the old block.
    mm_free(ptr);

    return new_ptr;
}

int mm_check(void) {
    void *bp = mem_heap_lo();

    while (GET_SIZE(HDRP(bp)) > 0) {
        // Check if the header and footer sizes match
        if (GET_SIZE(HDRP(bp)) != GET_SIZE(FTRP(bp))) {
            printf("Error: header and footer sizes do not match at %p\n", bp);
            return 0;
        }

        // Check if the header and footer allocation flags match
        if (GET_ALLOC(HDRP(bp)) != GET_ALLOC(FTRP(bp))) {
            printf("Error: header and footer allocation flags do not match at %p\n", bp);
            return 0;
        }

        // Check if there are any contiguous free blocks that escaped coalescing
        if (!GET_ALLOC(HDRP(bp)) && !GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
            printf("Error: contiguous free blocks found at %p and %p\n", bp, NEXT_BLKP(bp));
            return 0;
        }

        bp = NEXT_BLKP(bp);
    }

    // Check if the last block is the end of the heap
    if (bp != mem_heap_hi() + 1) {
        printf("Error: last block is not the end of the heap\n");
        return 0;
    }

    // Add any other consistency checks you find helpful

    return 1;
}