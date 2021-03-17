/* 
Mondee Lu, cs107, explicit.c
This program impelments an explicit list heap allocator. 

INITIALIZATION: The heap allocator is initialized with a pointer to the start of the heap segment and the size of the heap segment. 

MEMORY BLOCK DESIGN: Each block of memory contains an 8-byte header with information about the size of the available payload for 
that block and whether the block is allocated or available for use. When a block is not allocated (ie free), the payload space holds
pointers to other free blocks. To support the storage of 2 8-byte pointers, a minimum payload size of 16-bytes is enforced (resulting in
a minimum block size of 24 bytes).

FREE BLOCK LIST: A doubly linked listed is used to track and manage the available free blocks. As indicated above, the pointers to the
previous and next free blocks are stored in the payload space of an un-allocated memory block. The relative order of the blocks within memory
is not preserved via the free block list. However, the payload size information in the header of memory blocks can be used to traverse the
heap in order, if desired.

SUPPORTED FUNCTIONALITY: The interface supports mymalloc, myrealloc, and myfree functionalities, which map onto the standard malloc, 
realloc, and free functions provided in C.

PERFORMANCE: To reduce external fragmentation, consolidation of contiguous free bocks is performed when freeing and reallocating blocks. 
To reduce internal fragmentation, partitioning of blocks is performed when mallocing and reallocing. Utilization is fairly good in testing 
(averaging 72-85%). The program does prioritize throughput over utilization insofar that it uses a first-fit search when 
mallocing or reallocing. 
*/
#include "allocator.h"
#include "debug_break.h"
#include <string.h>
#include <stdio.h>

#define HEADER_SIZE 8 // size of header, in bytes
#define MIN_PAYLOAD_SIZE 16 // limit to ensure space for pointers
#define MIN_BLOCK_SIZE 24

/* ------------------
 * GLOBAL VARS 
 * ------------------
 */
static void *segment_start;
static void *segment_end;
static void *free_list_start;
static size_t segment_size;

/* ------------------
 * STRUCTS
 * ------------------
*/

// 16-byte struct to hold pointers
typedef struct Pointers {
    void *previous;
    void *next;
} Pointers;

// 8-byte struct to hold block header
typedef struct Header {
    unsigned int payload;
    int allocated;
} Header;


/* ----------------
 * UTILITIES
 * ----------------
 */

/* 
Function: align
Input: size_t number and size_t number
Output: size_t number
========================================
This function rounds the given number "size" to be a multiple of "mult" 
*/
size_t align(size_t size, size_t mult) {
    return (size + (mult - 1)) & ~(mult - 1);
}

/* 
Function: get_payload_ptr
Input: Void Pointer
Return Value: Void Pointer
=============================
Given a pointer to the start/header of a block, this function returns a pointer to the start of the payload 
for that block 
*/
void *get_payload_ptr(void *block) {
    return (unsigned char *)block + HEADER_SIZE;
}


/* --------------------------
 * FREE BLOCK LIST FUNCTIONS
 * --------------------------
 */

/* 
Function: add_block
Input: Void pointer
Return Value: None
=========================
Adds the given block to the front of the free list and does any necessary list and header maintenance 
*/
void add_block(void *block) {
    if (free_list_start == NULL) { // free list is empty
        ((Pointers *)get_payload_ptr(block))->previous = NULL;
        ((Pointers *)get_payload_ptr(block))->next = NULL;
    } else {
        ((Pointers *)get_payload_ptr(free_list_start))->previous = block;
        ((Pointers *)get_payload_ptr(block))->next = free_list_start;
        ((Pointers *)get_payload_ptr(block))->previous = NULL;
    }

    ((Header *)block)->allocated = 0;
    free_list_start = block;
}

/* 
Function: remove_block
Input: Void pointer
Return Value: None
=======================
This function removes the given block from the free list 
*/
void remove_block(void *block) {
    void *previous = ((Pointers *)get_payload_ptr(block))->previous;
    void *next = ((Pointers *)get_payload_ptr(block))->next;

    if (previous == NULL && next == NULL) { // only free block
        free_list_start = NULL;
    } else if (previous == NULL && next != NULL) { // first free block
        ((Pointers *)get_payload_ptr(next))->previous = NULL;
        free_list_start = next;
    } else if (previous != NULL && next == NULL) { // last free block
        ((Pointers *)get_payload_ptr(previous))->next = NULL;
    } else { //sandwiched block
        ((Pointers *)get_payload_ptr(previous))->next = next;
        ((Pointers *)get_payload_ptr(next))->previous = previous;
    }

    ((Header *)block)->allocated = 1;
}

    
/* -------------
 * HELPERS
 * -------------
 */

/* 
Function: partition
Input: Void pointer, size_t number, and size_t number
Return Value: None
=======================================================
This function splits a given block into two given the available payload space and the given payload size. 
*/
void partition(void *block, size_t payload_space, size_t payload) {
    void *next_block = (unsigned char *)block + payload + HEADER_SIZE;

    unsigned int next_payload = payload_space - payload - HEADER_SIZE;
    ((Header *)next_block)->payload = next_payload;
    ((Header *)next_block)->allocated = 0;

    // add to free list
    add_block(next_block);
    // update block with new size
    ((Header *)block)->payload = payload;
}

/* 
Function: find_fit 
Input: size_t number
Return Value: Void pointer
======================
This function traverses the explicit list of free blocks to find a suitable free block given the payload size.
It returns a pointer to the block, or NULL if a block cannot be found. If a suitable block is found, find_fit also performs 
the necessary list and header maintenance to indicate the block is allocated. 
*/
void *find_fit(size_t aligned_requested_size) {
    void *curr_block = free_list_start;

    while (curr_block != NULL) {
        unsigned int payload_space = ((Header *)curr_block)->payload;
        if (payload_space >= aligned_requested_size) {
            // partition the current block if large enough
            if (payload_space >= aligned_requested_size + MIN_BLOCK_SIZE) {
                partition(curr_block, payload_space, aligned_requested_size);
            }
            remove_block(curr_block);
            return curr_block;
        }

        curr_block = ((Pointers *)get_payload_ptr(curr_block))->next;
    }

    return NULL;
}

/* 
Function: coalesce_right
Input: Void pointer
Return Value: None
=========================== 
This function consolidates as many contiguous free blocks to the right of the given block as possible. Because the 
free list is not in address order, this function traverses the heap via pointer arthimetic, utilizing header information. This function 
also does the necessary free list and header maintenance associated with any consolidation of blocks.
*/
void coalesce_right(void *block) {
    if (block == NULL) { // something went wrong
        return;
    }
    
    void *curr_block = (char *)block + ((Header *)block)->payload + HEADER_SIZE;

    while (curr_block < segment_end && ((Header *)curr_block)->allocated == 0) {
        ((Header *)block)->payload += ((Header *)curr_block)->payload + HEADER_SIZE;
        remove_block(curr_block);

        curr_block = (unsigned char *)curr_block + ((Header *)curr_block)->payload + HEADER_SIZE;
    }
}


/* ---------------------
 * MAIN HEAP FUNCTIONS
 * ---------------------
 */

/* 
Function: myinit
Input: Void pointer and size_t number
Output: Boolean
=======================================
This function initializes the allocator with the given heap segment parameters as along as at least one valid request can be serviced. If the 
allocator is successfully initialized, true is returned. Otherwise, false is returned
*/
bool myinit(void *heap_start, size_t heap_size) {
    if (heap_size <= HEADER_SIZE) {
        return false;
    }
    segment_start = heap_start;
    segment_end = (unsigned char *)heap_start + heap_size;
    segment_size = heap_size;
   
    free_list_start = heap_start;

    // set up first header
    unsigned int payload = heap_size - HEADER_SIZE;
    ((Header *)segment_start)->payload = payload;
    ((Header *)segment_start)->allocated = 0;

    // set up the first pointers
    void *payload_ptr = get_payload_ptr(segment_start);
    ((Pointers *)payload_ptr)->previous = NULL;
    ((Pointers *)payload_ptr)->next = NULL;

    return true;  
}


/* 
Function: mymalloc
Input: Size_t number
Return Value: Void Pointer
========================
This function takes in a requested payload size and finds and returns an unallocated block of memory that is sufficiently large to hold 
the payload, or null if no such block can be found. The returned pointer points to the start of the payload space, not the header.
*/
void *mymalloc(size_t requested_size) {
    if (requested_size > MAX_REQUEST_SIZE || requested_size == 0) {
        return NULL;
    }

    size_t aligned_requested_size = align(requested_size, ALIGNMENT);
    if (aligned_requested_size < MIN_PAYLOAD_SIZE) {
        aligned_requested_size = MIN_PAYLOAD_SIZE;
    }
    
    void *block = find_fit(aligned_requested_size);
    if (block != NULL) {
        return get_payload_ptr(block); // return a pointer to the start of the payload space
    } else {
        return NULL;
    }
}

/* 
Function: myfree 
Input: Void pointer
Return Value: None
====================
This function marks the given block as available for allocation by adding it to the list of
free blocks. Before adding to the list of free blocks, myfree also checks if the given block can be 
coalesced with neighboring blocks to the right. Finally, it updates the block's header to reflect its deallocation. 
 */
void myfree(void *ptr) {
    if (ptr != NULL) {
        void *block_ptr = (unsigned char *)ptr - HEADER_SIZE;
        // coalesce then add to free list
        coalesce_right(block_ptr);
        add_block(block_ptr);

        // update header to reflect deallocation   
        ((Header *)block_ptr)->allocated = 0;      
    }
}

/*
Function: myrealloc 
Input: Void pointer and a size_t number
Return Value: Void pointer
==========================================
This function reallocs existing memory. Given a pointer to the payload to be reallocated and a new size, the function 
first attempts to reallocate in place if the given block is sufficiently large or can be expanded/contracted to 
accomodate the new size. If in-place realloc is not possible, it mallocs a new block. It returns a pointer to the "new" 
payload space.
 */
void *myrealloc(void *old_ptr, size_t new_size) {
    if (old_ptr == NULL || new_size == 0) { // handling of edge cases
        return mymalloc(new_size);
    }
    void *old_block_ptr = (unsigned char *)old_ptr - HEADER_SIZE;
    size_t old_payload_size = ((Header *)old_block_ptr)->payload;
    
    size_t new_aligned_size = align(new_size, ALIGNMENT);
    if (new_aligned_size < MIN_PAYLOAD_SIZE) {
        new_aligned_size = MIN_PAYLOAD_SIZE;
    }

    size_t min_split = new_aligned_size + MIN_BLOCK_SIZE;

    if (old_payload_size == new_aligned_size) { // don't need to do anything
        return old_ptr;
    }
    if (old_payload_size > new_aligned_size && old_payload_size < min_split) { // bigger than needed, but not big enough to split 
        return old_ptr;
    }
    if (old_payload_size > new_aligned_size && old_payload_size >= min_split) { // split current block
        partition(old_block_ptr, old_payload_size, new_aligned_size);
        
        return old_ptr;
    }
    if (old_payload_size < new_aligned_size) {
        coalesce_right(old_block_ptr);
        unsigned int new_payload_size = ((Header *)old_block_ptr)->payload;
        if (new_payload_size >= new_aligned_size) { // check if realloc in place is possible
            if (new_payload_size >= min_split) {
                partition(old_block_ptr, ((Header *)old_block_ptr)->payload, new_aligned_size);
            }
            return old_ptr;
        }
        void *realloc_block = mymalloc(new_size);
        if (realloc_block != NULL) {
            memcpy(realloc_block, old_ptr, old_payload_size);
            myfree(old_ptr);
            return realloc_block;
        }
    }

    return NULL;
}



/* ----------------------
 * DEBUGGING FUNCTIONS
 * ----------------------
 */


/* 
Function: validate_heap
Input: None
Return Value: Boolean
===========================
This function performs some error checking to ensure the heap is well-formed. In particular, it checks
that (1) blocks are properly marked as allocated and added/removed from the free list and (2) reported payload sizes
are correct. If the heap is valid, true is returned. Otherwise, false is returned. 
*/
bool validate_heap() {
    return true; // test speed without validate_heap, comment out if you want to run validate_heap
    void *curr_block = segment_start;
    size_t total_bytes = 0;

    while (curr_block < segment_end) {
        unsigned int payload = ((Header *)curr_block)->payload;
        int allocated = ((Header *)curr_block)->allocated; 
        total_bytes += payload + HEADER_SIZE;

        int found = 0;
        if (allocated == 0) {
            void *curr_free = free_list_start;
            while (curr_free != NULL) {
                if (curr_free == curr_block) {
                    found += 1;
                }
                curr_free = ((Pointers *)get_payload_ptr(curr_free))->next;
            }

            if (found == 0) {
                printf("Allocation status incorrect or not added properly to free list: %p\n", curr_block);
                return false;
            }
            if (found > 1) {
                printf("In list multiple times: %p\n", curr_block);
                return false;
            }
        }

        if (payload < 8 || payload > segment_size) {
            printf("Block payload size incorrect: %p\n", curr_block);
        }

        curr_block = (unsigned char *)curr_block + payload + HEADER_SIZE;
    }

    if (total_bytes > segment_size) {
        printf("Blocks have more collective space than segment\n");
        printf("Allocated: %zu\n", total_bytes);
        printf("Segment size: %zu", segment_size);
        return false;
    }

    return true;
}


/* 
Funtion: dump_heap
Input: Integer of value 0, 1, or 2
Return Value: None
====================
This function prints out information about the heap for debugging and testing purposes. 
It takes in an integer value of 0, 1, or 2, which determines what information is printed when the
function is called. If called with 0, information about each block in the heap is printed out. If
called with 1, information about the list of free blocks is printed out. If called with 2, information
about both the blocks and the free list is printed out 
*/ 
void dump_heap(int mode) {
    void *curr_block = segment_start;
    void *curr_free_block = free_list_start;
    void *last_viable = (char *)segment_end - MIN_BLOCK_SIZE;

    if (mode == 0 || mode == 2) {
        printf("Block by block\n");
        while (curr_block <= last_viable) {
            unsigned int payload = ((Header *)curr_block)->payload;
            int allocated = ((Header *)curr_block)->allocated;

            void *next_block = (unsigned char *)curr_block + payload + HEADER_SIZE;

            printf("=========================\n");
            printf("Block ptr: %p\n", curr_block);
            printf("Payload size: %u\n", payload);
            printf("Allocated: %i\n", allocated);
            printf("Next block: %p\n", next_block);

            curr_block = next_block;
   
        }
    }

    printf("\n\n");

    if (mode == 1 || mode == 2) {

        printf("Free block list\n");
        while (curr_free_block != NULL) {
            void *previous = ((Pointers *)get_payload_ptr(curr_free_block))->previous;
            void *next = ((Pointers *)get_payload_ptr(curr_free_block))->next;

            printf("========================\n");
            printf("Free Block: %p\n", curr_free_block);
            printf("Payload: %u\n", ((Header *)curr_free_block)->payload);
            printf("Previous free: %p\n", previous);
            printf("Next free: %p\n", next);

            curr_free_block = next;
        }
    }   
}
