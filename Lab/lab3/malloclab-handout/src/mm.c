/*
 * mm.c - Explicit free list, first-fit memory allocator.
 * 
 * This allocator uses an explicit free list to keep track of free memory blocks.
 * Each free block contains a header and footer, as well as pointers to the
 * previous and next free blocks in the free list. Allocated blocks have a header
 * and footer, but do not contain pointers to other blocks.
 * 
 * The free list is organized as a singly linked list, using a first-fit strategy
 * to search for an appropriate block when allocating memory. When a block is
 * freed, the allocator attempts to coalesce it with adjacent free blocks to form
 * larger contiguous free blocks.
 * 
 * Each function in this allocator is preceded by a header comment that describes
 * what the function does, its inputs, outputs, and any side effects.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name : Your student ID */
    "2019-11730",
    /* Your full name */
    "Hyeonji Shin",
    /* Your student ID */
    "2019-11730",
    /* leave blank */
    "",
    /* leave blank */
    ""
};

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

/* Given block ptr bp, compute address of next and previous free blocks */
#define NEXT_FREE_BLKP(bp) (*(char **)(bp))
#define PREV_FREE_BLKP(bp) (*(char **)(bp + WSIZE))

#define SET_NEXT_FREE_BLKP(bp, next) (NEXT_FREE_BLKP(bp) = (char *)next)
#define SET_PREV_FREE_BLKP(bp, prev) (PREV_FREE_BLKP(bp) = (char *)prev)

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static void *extend_heap(size_t words);
static void *find_fit(size_t size);
static void place(void *bp, size_t size);
static void *coalesce(void *bp);
static void insert_free_block(char *bp);
static void remove_free_block(char *bp);

static int mm_check(void);

/* Global variables */
static char *heap_listp = NULL;
static char *freelist_head = NULL;

/* 
 * mm_init - Initialize the memory allocator.
 * This function is responsible for setting up the initial empty heap and
 * extending it with a free block of CHUNKSIZE bytes. It initializes the return coalesce(bp);
 */
int mm_init(void)
{
    // Allocate initial heap space
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *)-1) {
        return -1;
    }

    // Set up prologue and epilogue blocks
    PUT(heap_listp, 0);                                /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(2 * DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), (int)NULL);          /* Prev free block */
    PUT(heap_listp + (3 * WSIZE), (int)NULL);          /* Next free block */
    PUT(heap_listp + (4 * WSIZE), PACK(2 * DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));         /* Epilogue header */

    heap_listp += (2 * WSIZE); // Move heap pointer past prologue blocks
    freelist_head = heap_listp; // Set the head of the free list

    // Extend heap
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }

    // mm_check(); // Debugging tool to check for heap consistency
    return 0;
}

/* 
 * mm_malloc - Allocate a block of memory.
 * This function takes a requested size and rounds it up to the nearest
 * multiple of the alignment. It then searches the free list for an appropriate
 * free block using the first-fit strategy. If no suitable block is found, it
 * extends the heap and allocates the block in the extended heap. Returns a
 * pointer to the allocated block.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) {
        return NULL;
    }

    // Adjust block size to include overhead and alignment reqs.
    asize = ALIGN(size + SIZE_T_SIZE);

    // Search the free list for a fit
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // No fit found. Get more memory and place the block
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);

    // mm_check(); // Debugging tool to check for heap consistency
    return bp;
}

/*
 * mm_free - Free a previously allocated block of memory.
 * This function frees a given block of memory by marking it as unallocated in
 * the block header and footer. It then attempts to coalesce the newly freed
 * block with adjacent free blocks to create larger contiguous free blocks.
 * The coalesced block is inserted into the free list.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    // Mark block as free
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    // Coalesce if the previous block was free
    coalesce(ptr);

    // mm_check(); // Debugging tool to check for heap consistency
}

/*
 * mm_realloc - Resize an allocated block of memory.
 * This function resizes a given block of memory to a new size. If the new size
 * is smaller than the current size, it shrinks the block and returns the same
 * pointer. If the new size is larger than the current size, it allocates a new
 * block of the requested size, copies the data from the old block to the new
 * block, frees the old block, and returns a pointer to the new block. If the
 * given pointer is NULL, it behaves like mm_malloc. If the new size is 0, it
 * behaves like mm_free.
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    // If ptr is NULL, the call is equivalent to mm_malloc(size)
    if (ptr == NULL) {
        return mm_malloc(size);
    }

    // If size is equal to zero, the call is equivalent to mm_free(ptr)
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    newptr = mm_malloc(size);
    if (newptr == NULL) {
        return NULL;
    }

    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize) {
        copySize = size;
    }

    // Copy the data from oldptr to newptr
    memcpy(newptr, oldptr, copySize);
    // Free the old block
    mm_free(oldptr);

    // mm_check(); // Debugging tool to check for heap consistency
    return newptr;
}


// ==================== Helper functions ==================== //

/*
 * extend_heap - Extends the heap and initializes the new free block.
 */
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

/*
 * coalesce - Coalesces the given block with adjacent free blocks.
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    
    // Case 1: Both previous and next blocks are allocated
    if (prev_alloc && !next_alloc) { // Case 2: Previous block is allocated, next block is free
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_free_block(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    
    else if (!prev_alloc && next_alloc) { // Case 3: Previous block is free, next block is allocated
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        remove_free_block(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    
    else if (!prev_alloc && !next_alloc) { // Case 4: Both previous and next blocks are free
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        remove_free_block(PREV_BLKP(bp));
        remove_free_block(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    insert_free_block(bp);
    return bp;
}

/*
 * find_fit - Finds the first-fit free block for the given size.
 */
static void *find_fit(size_t size)
{
    void *bp;

    // First-fit search
    for (bp = freelist_head; GET_ALLOC(HDRP(bp)) == 0; bp = NEXT_FREE_BLKP(bp)) {
        if (GET_SIZE(HDRP(bp)) >= size) {
            return bp;
        }
    }

    return NULL;
}

/*
 * place - Places a requested block and removes it from the free list.
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    // Remove the block from the free list
    remove_free_block(bp);

    // If the remainder would be at least the minimum block size, split the block
    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        insert_free_block(bp);
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * insert_free_block - Inserts a free block into the free list.
 */
static void insert_free_block(char *bp)
{
    SET_PREV_FREE_BLKP(bp, NULL);
    SET_NEXT_FREE_BLKP(bp, freelist_head);

    if (freelist_head != NULL) {
        SET_PREV_FREE_BLKP(freelist_head, bp);
    }

    freelist_head = bp;
}

/*
 * remove_free_block - Removes a free block from the free list.
 */
static void remove_free_block(char *bp)
{
    if (PREV_FREE_BLKP(bp)) {
        SET_NEXT_FREE_BLKP(PREV_FREE_BLKP(bp), NEXT_FREE_BLKP(bp));
    } else {
        freelist_head = NEXT_FREE_BLKP(bp);
    }

    if (NEXT_FREE_BLKP(bp)) {
        SET_PREV_FREE_BLKP(NEXT_FREE_BLKP(bp), PREV_FREE_BLKP(bp));
    }
}


// ==================== Heap Check functions ==================== //

/* in_heap - Check if a pointer is within the heap range */
static int in_heap(void *ptr) {
    return (ptr >= mem_heap_lo() && ptr <= mem_heap_hi());
}

/* aligned - Check if a pointer is aligned */
static int aligned(void *ptr) {
    return ((size_t)ptr % DSIZE == 0);
}

/*
 * Perform a set of checks to verify that the heap is consistent.
 *
 * To use this function, simply call it from within mm_init, mm_malloc, mm_free, or mm_realloc
 * by uncommenting the function call.
 * 
 * Specifically, check for:
 * Allocated blocks in the free list
 * Contiguous free blocks that escaped coalescing
 * Free blocks not actually in the free list
 * Pointers in the free list that don't point to valid free blocks
 * Allocated blocks that overlap
 * Pointers in heap blocks that don't point to valid heap addresses
 *
 * @return 1 if the heap is consistent, 0 otherwise.
 */
static int mm_check(void)
{
    void *bp;

    // Check if every block in the free list is marked as free
    for (bp = freelist_head; NEXT_FREE_BLKP(bp) != NULL; bp = NEXT_FREE_BLKP(bp)) {
        if (GET_ALLOC(HDRP(bp)) != 0) {
            printf("Error: Allocated block in the free list.\n");
            return 0;
        }
    }

    // Check if there are contiguous free blocks that escaped coalescing
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && !GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
            printf("Error: Contiguous free blocks not coalesced.\n");
            return 0;
        }
    }

    // Check if every free block is actually in the free list
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp))) {
            void *fp;
            int found = 0;
            for (fp = freelist_head; NEXT_FREE_BLKP(fp) != NULL; fp = NEXT_FREE_BLKP(fp)) {
                if (fp == bp) {
                    found = 1;
                    break;
                }
            }

            if (!found) {
                printf("Error: Free block not in the free list.\n");
                return 0;
            }
        }
    }

    // Check if pointers in the free list point to valid free blocks
    for (bp = freelist_head; NEXT_FREE_BLKP(bp) != NULL; bp = NEXT_FREE_BLKP(bp)) {
        if (!in_heap(bp) || !aligned(bp)) {
            printf("Error: Pointer in the free list is not a valid free block.\n");
            return 0;
        }
    }

    // Check if allocated blocks overlap
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (GET_ALLOC(HDRP(bp)) && GET_ALLOC(HDRP(NEXT_BLKP(bp))) &&
            ((char *)bp + GET_SIZE(HDRP(bp)) > (char *)NEXT_BLKP(bp))) {
            printf("Error: Allocated blocks overlap.\n");
            return 0;
        }
    }

    // Check if pointers in heap blocks point to valid heap addresses
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!in_heap(bp) || !aligned(bp)) {
            printf("Error: Pointer in heap block is not a valid heap address.\n");
            return 0;
        }
    }

    return 1;
}
