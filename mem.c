#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "mem.h"

/*
 * This structure serves as the header for each allocated and free block
 * It also serves as the footer for each free block
 * The blocks are ordered in the increasing order of addresses
 */
typedef struct blk_hdr {
    int size_status;
    
    /*
     * Size of the block is always a multiple of 8
     * => last two bits are always zero - can be used to store other information
     *
     * LSB -> Least Significant Bit (Last Bit)
     * SLB -> Second Last Bit
     * LSB = 0 => free block
     * LSB = 1 => allocated/busy block
     * SLB = 0 => previous block is free
     * SLB = 1 => previous block is allocated/busy
     *
     * When used as the footer the last two bits should be zero
     */
    
    /*
     * Examples:
     *
     * For a busy block with a payload of 20 bytes (i.e. 20 bytes data + an additional 4 bytes for header)
     * Header:
     * If the previous block is allocated, size_status should be set to 27
     * If the previous block is free, size_status should be set to 25
     *
     * For a free block of size 24 bytes (including 4 bytes for header + 4 bytes for footer)
     * Header:
     * If the previous block is allocated, size_status should be set to 26
     * If the previous block is free, size_status should be set to 24
     * Footer:
     * size_status should be 24
     *
     */
} blk_hdr;

/* Global variable - This will always point to the first block
 * i.e. the block with the lowest address */
blk_hdr *first_blk = NULL;

/*
 * Note:
 *  The end of the available memory can be determined using end_mark
 *  The size_status of end_mark has a value of 1
 *
 */

/*
 * Function for allocating 'size' bytes
 * Returns address of allocated block on success
 * Returns NULL on failure
 * Here is what this function should accomplish
 * - Check for sanity of size - Return NULL when appropriate
 * - Round up size to a multiple of 8
 * - Traverse the list of blocks and allocate the best free block which can accommodate the requested size
 * - Also, when allocating a block - split it into two blocks
 * Tips: Be careful with pointer arithmetic
 */
void* Mem_Alloc(int size) {
    // Your code goes in here
    
    // return null if the size required is 0
    if (size == 0){
        return NULL;
    }
    
    // Satisfy double word alignment
    // Increment size to include the header
    size += 4;
    // add padding if needed
    if(size % 8 != 0){
        size = (size / 8 + 1) * 8;
    }
    
    // initialize a pointer to point to the first block
    blk_hdr* ptr = first_blk;
    // initialize minimum size to be -1 indicating no minimum block found
    int minSize = -1;
    // initialize minimum block to point to the first block
    blk_hdr* minBlock = first_blk;
    
    // Check if the end mark has been reached
    while(ptr->size_status != 1){
        // if current block is allocated
        if( (ptr->size_status & 1) == 1){
            // if the previous block is allocated
            if( (ptr->size_status & 3) == 3){
                // get the current size of the current block
                int currSize = ptr->size_status - 3;
                // skip to the next header
                ptr = (void*)(ptr + currSize);
            }
            // if the previous block is not allocated
            else{
                // get the current size of the current block
                int currSize = ptr->size_status-1;
                // skip to the next header
                ptr = (void*)(ptr + currSize);
                Mem_Dump();
            }
        }
        
        // if the current header is free and its size is bigger than requested
        else if( ptr->size_status > size + 2 ){
            // if it's the first free block that we encounter
            if(minSize == -1){
                // set the current minimum block to point to ptr
                minBlock = ptr;
                // set the minimum size to be the size of current minimum block size
                minSize = minBlock->size_status - 2;
            }
            // if it's not the first free block that we encounter
            else{
                // if the previous block is allocated
                if( (ptr->size_status & 2) == 2){
                    // check size of the current block and compare it to minimum block
                    // if size of the current block is smaller than minimum block
                    if( (ptr->size_status - 2) < minSize){
                        // update minSize
                        minSize = ptr->size_status - 2;
                        // update minBlock pointer
                        minBlock = ptr;
                    }
                }
                // if the previous block is free
                else{
                    // check size of the current block and compare it to minimum block
                    // if size of the current block is smaller than minimum block
                    if( (ptr->size_status) < minSize){
                        // update minSize
                        minSize = ptr->size_status;
                        // update minBlock
                        minBlock = ptr;
                    }
                }
            }
        }
        
        //    if the current is free and the size is equal to requested
        else if( ptr->size_status == size || ptr->size_status-2 == size){
            // change its header's status
            ptr->size_status += 1;
            //return the address of the payload
            return (void*)(ptr + 4);
        }
        
        //    if the current block is free but size is smaller than requested
        else{
            // if the previous block is allocated
            if( (ptr->size_status & 2) == 2){
                // get the size of the current block
                int currSize = ptr->size_status - 2;
                // skip to the next header
                ptr = (void*)(ptr + currSize); // ()?
            }else{
                // if the previous block is free
                int currSize = ptr->size_status;
                // skip to the next header
                ptr = (void*)(ptr + currSize);
            }
        }
    }
    
    // if found a minimum block
    if(minSize != -1){
        // Do splitting
        // if the previous block of minBlock is allocated
        if( (minBlock->size_status & 2) == 2){
            // update minBlock's header
            minBlock->size_status = size + 3;
        }
        // if the previous block is free
        else{
            // update minBlock's header
            minBlock->size_status = size + 1;
        }
        // initialize nextBlock to point to the next free block from splitting
        blk_hdr* nextBlock = (void*)(minBlock + size);
        // update header of next free block
        nextBlock->size_status = minSize - size + 2;
        // initialize footer to point to the footer of minBlock
        blk_hdr* footer = (void*)(minBlock + minSize - 4);
        //update the footer
        footer->size_status = minSize - size;
        // return the payload address of minimum block
        return (void*)(minBlock + 4);
    }
    // if no such block is found
    return NULL;
    
}

/*
 * Function for freeing up a previously allocated block
 * Argument - ptr: Address of the block to be freed up
 * Returns 0 on success
 * Returns -1 on failure
 * Here is what this function should accomplish
 * - Return -1 if ptr is NULL
 * - Return -1 if ptr is not 8 byte aligned or if the block is already freed
 * - Mark the block as free
 * - Coalesce if one or both of the immediate neighbours are free
 */
int Mem_Free(void *ptr) {
    // Your code goes in here
    
    //    // if the ptr is NULL
    //    if(ptr == NULL){
    //        return -1;
    //    }
    //
    //    // if the ptr is not 8 byte aligned
    //    if( ((unsigned int)ptr&8) != 8 && ((unsigned int)ptr&0) != 0){
    //        return -1;
    //    }
    //
    //    // get the size_status of the header of the block that ptr points to
    //    blk_hdr *currBlock = (blk_hdr*)ptr - 4; // Do I need to typecast ptr here?
    //    int currSizeStatus = currBlock->size_status; // can size_status already be read?
    //
    //    // if the block ptr points to is already a free block
    //    if( (currSizeStatus & 1) == 0){
    //        return -1;
    //    }
    //
    //    // if the previous block is allocated
    //    if( (currSizeStatus & 2) == 2){
    //        // get the size of the current block
    //        int currSize = currSizeStatus - 3; //NEED TO CHECK WHETHER -2 OR -3
    //        // get the block header of the next block
    //        blk_hdr *nextBlock = currBlock + (currSize/4); // Does (blk_hdr*) jumps 4 bytes?
    //        // read the size_status of the next block
    //        int nextSizeStatus = nextBlock->size_status;
    //
    //        // if the next block is allocated
    //        if( (nextSizeStatus & 1) == 1){
    //            // jump 4 bytes back to set the footer of current block that is being freed
    //            blk_hdr *footer = nextBlock-1; // CONFIRM AUTOSCALING!!
    //            footer->size_status = currSize;
    //            // Null out ptr
    //            ptr = NULL;
    //            // return to the current block header to reset the header
    //        }
    //        // if the next block is free, coalesce it
    //        else{
    //            // get the size of the next block
    //            int nextSize = nextSizeStatus - 2;
    //            // set the footer
    //            blk_hdr *footer = nextBlock + (nextSize-4) / 4;
    //            footer->size_status = currSize + nextSize;
    //            // go back to update the current block's header
    //            currBlock->size_status = footer->size_status + 2;
    //            // Null out ptr
    //            ptr = NULL; // Do I need to null out ptr?
    //        }
    //    }
    //
    //    //if the previous block is free
    //    else{
    //        // get the size of the current block
    //        int currSize = currSizeStatus -1;
    //        // get the previous block size by its footer
    //        int prevSize = (currBlock - 1)->size_status; // Correct way to get the size by footer?
    //        // a pointer to the previous block header
    //        blk_hdr *prevBlock = currBlock - (prevSize/4);
    //        // get the block header of the next block
    //        blk_hdr *nextBlock = currBlock + (currSize/4);
    //        // read the size_status of the next block
    //        int nextSizeStatus = nextBlock->size_status;
    //
    //        // if the next block is allocated
    //        if( (nextSizeStatus & 1) == 1){
    //            // change the previous block header's size_status
    //            prevBlock->size_status += currSize;
    //            // set the footer of the new bigger free block
    //            blk_hdr *footer = nextBlock - 1; // CHECK AUTOSCALING
    //            footer->size_status = currSize + prevSize;
    //            // null out ptr
    //            ptr = NULL;
    //        }
    //
    //        //if the next block is free
    //        else{
    //            // get the size of the next block
    //            int nextSize = nextSizeStatus - 2;
    //            // change the previous block header's size_status
    //            prevBlock->size_status += (currSize + nextSize);
    //            // set the footer of the new bigger free block
    //            blk_hdr *footer = nextBlock + (nextSize-4) / 4;
    //            footer->size_status = prevSize + currSize + nextSize;
    //            // null out ptr
    //            ptr = NULL;
    //        }
    //    }
    //    // return 0 when success
    return 0;
}

/*
 * Function used to initialize the memory allocator
 * Not intended to be called more than once by a program
 * Argument - sizeOfRegion: Specifies the size of the chunk which needs to be allocated
 * Returns 0 on success and -1 on failure
 */
int Mem_Init(int sizeOfRegion) {
    int pagesize;
    int padsize;
    int fd;
    int alloc_size;
    void* space_ptr;
    blk_hdr* end_mark;
    static int allocated_once = 0;
    
    if (0 != allocated_once) {
        fprintf(stderr,
                "Error:mem.c: Mem_Init has allocated space during a previous call\n");
        return -1;
    }
    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }
    
    // Get the pagesize
    pagesize = getpagesize();
    
    // Calculate padsize as the padding required to round up sizeOfRegion
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;
    
    alloc_size = sizeOfRegion + padsize;
    
    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    space_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE,
                     fd, 0);
    if (MAP_FAILED == space_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }
    
    allocated_once = 1;
    
    // for double word alignement and end mark
    alloc_size -= 8;
    
    // To begin with there is only one big free block
    // initialize heap so that first block meets
    // double word alignement requirement
    first_blk = (blk_hdr*) space_ptr + 1;
    end_mark = (blk_hdr*)((void*)first_blk + alloc_size);
    
    // Setting up the header
    first_blk->size_status = alloc_size;
    
    // Marking the previous block as busy
    first_blk->size_status += 2;
    
    // Setting up the end mark and marking it as busy
    end_mark->size_status = 1;
    
    // Setting up the footer
    blk_hdr *footer = (blk_hdr*) ((char*)first_blk + alloc_size - 4);
    footer->size_status = alloc_size;
    
    return 0;
}

/*
 * Function to be used for debugging
 * Prints out a list of all the blocks along with the following information i
 * for each block
 * No.      : serial number of the block
 * Status   : free/busy
 * Prev     : status of previous block free/busy
 * t_Begin  : address of the first byte in the block (this is where the header starts)
 * t_End    : address of the last byte in the block
 * t_Size   : size of the block (as stored in the block header) (including the header/footer)
 */
void Mem_Dump() {
    int counter;
    char status[5];
    char p_status[5];
    char *t_begin = NULL;
    char *t_end = NULL;
    int t_size;
    
    blk_hdr *current = first_blk;
    counter = 1;
    
    int busy_size = 0;
    int free_size = 0;
    int is_busy = -1;
    
    fprintf(stdout, "************************************Block list***\
            ********************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout, "-------------------------------------------------\
            --------------------------------\n");
    
    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;
        
        if (t_size & 1) {
            // LSB = 1 => busy block
            strcpy(status, "Busy");
            is_busy = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "Free");
            is_busy = 0;
        }
        
        if (t_size & 2) {
            strcpy(p_status, "Busy");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "Free");
        }
        
        if (is_busy)
            busy_size += t_size;
        else
            free_size += t_size;
        
        t_end = t_begin + t_size - 1;
        
        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%d\n", counter, status,
                p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);
        
        current = (blk_hdr*)((char*)current + t_size);
        counter = counter + 1;
    }
    
    fprintf(stdout, "---------------------------------------------------\
            ------------------------------\n");
    fprintf(stdout, "***************************************************\
            ******************************\n");
    fprintf(stdout, "Total busy size = %d\n", busy_size);
    fprintf(stdout, "Total free size = %d\n", free_size);
    fprintf(stdout, "Total size = %d\n", busy_size + free_size);
    fprintf(stdout, "***************************************************\
            ******************************\n");
    fflush(stdout);
    
    return;
}
