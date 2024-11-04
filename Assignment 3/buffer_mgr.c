#include<stdio.h>
#include<stdlib.h>
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include <math.h>

typedef struct Page
{
    SM_PageHandle data; // Actual data of the page
    PageNumber pageNum; // An identification integer given to each page
    int dirtyBit; // Used to indicate whether the contents of the page has been modified by the client
    int fixCount; // Used to indicate the number of clients using that page at a given instance
    int pade_addNum;   // Used by LRU algorithm to get the least recently used page  
    int refNum;   // Used by LFU algorithm to get the least frequently used page
} PageFrame;

int buffer_size = 0;
int read_index = 0;
int write_count = 0;
int pade_add = 0;
int clockPointer = 0;
int lfuPointer = 0;

extern void FIFO(BM_BufferPool *const bm, PageFrame *page)
{

    PageFrame *pageFrame = (PageFrame *) bm->mgmtData;
    
    int i, front_index;
    front_index = read_index % buffer_size;

    // Interating through all the page frames in the buffer pool
    for(i = 0; i < buffer_size; i++)
    {
        if(pageFrame[front_index].fixCount == 0)
        {
            // If page in memory is been  modified, then write page 
            if(pageFrame[front_index].dirtyBit == 1)
            {
                SM_FileHandle fh;
                openPageFile(bm->pageFile, &fh);
                writeBlock(pageFrame[front_index].pageNum, &fh, pageFrame[front_index].data);
                
                // Increase the write_count which records the number of writes done by the buffer manager.
                write_count++;
            }
            
            // Setting page frame's content to new page's content
            pageFrame[front_index].data = page->data;
            pageFrame[front_index].pageNum = page->pageNum;
            pageFrame[front_index].dirtyBit = page->dirtyBit;
            pageFrame[front_index].fixCount = page->fixCount;
            break;
        }
        else
        {
            // If the current page frame is being used by some client, we move on to the next location
            front_index++;
            front_index = (front_index % buffer_size == 0) ? 0 : front_index;
        }
    }
}

// Defining LFU (Least Frequently Used) function
extern void LFU(BM_BufferPool *const bm, PageFrame *page)
{
    //printf("LFU Started");
    PageFrame *pageFrame = (PageFrame *) bm->mgmtData;
    
    int i, j, leastFreqIndex, leastFreqRef;
    leastFreqIndex = lfuPointer;    
    
    // Interating through all the page frames in the buffer pool
    for(i = 0; i < buffer_size; i++)
    {
        if(pageFrame[leastFreqIndex].fixCount == 0)
        {
            leastFreqIndex = (leastFreqIndex + i) % buffer_size;
            leastFreqRef = pageFrame[leastFreqIndex].refNum;
            break;
        }
    }

    i = (leastFreqIndex + 1) % buffer_size;

    // Finding the page frame having minimum refNum (i.e. it is used the least frequent) page frame
    for(j = 0; j < buffer_size; j++)
    {
        if(pageFrame[i].refNum < leastFreqRef)
        {
            leastFreqIndex = i;
            leastFreqRef = pageFrame[i].refNum;
        }
        i = (i + 1) % buffer_size;
    }
        
    // If page in memory has been modified (dirtyBit = 1), then write page to disk  
    if(pageFrame[leastFreqIndex].dirtyBit == 1)
    {
        SM_FileHandle fh;
        openPageFile(bm->pageFile, &fh);
        writeBlock(pageFrame[leastFreqIndex].pageNum, &fh, pageFrame[leastFreqIndex].data);
        
        // Increase the write_count which records the number of writes done by the buffer manager.
        write_count++;
    }
    
    // Setting page frame's content to new page's content       
    pageFrame[leastFreqIndex].data = page->data;
    pageFrame[leastFreqIndex].pageNum = page->pageNum;
    pageFrame[leastFreqIndex].dirtyBit = page->dirtyBit;
    pageFrame[leastFreqIndex].fixCount = page->fixCount;
    lfuPointer = leastFreqIndex + 1;
}

// Defining LRU (Least Recently Used) function
extern void LRU(BM_BufferPool *const bm, PageFrame *page)
{   
    PageFrame *pageFrame = (PageFrame *) bm->mgmtData;
    int i, leastpade_addIndex=0, leastpade_addNum;

    // Interating through all the page frames in the buffer pool.
    for(i = 0; i < buffer_size; i++)
    {
        // Finding page frame whose fixCount = 0 i.e. no client is using that page frame.
        if(pageFrame[i].fixCount == 0)
        {
            leastpade_addIndex = i;
            leastpade_addNum = pageFrame[i].pade_addNum;
            break;
        }
    }   

    // Finding the page frame having minimum pade_addNum (i.e. it is the least recently used) page frame
    for(i = leastpade_addIndex + 1; i < buffer_size; i++)
    {
        if(pageFrame[i].pade_addNum < leastpade_addNum)
        {
            leastpade_addIndex = i;
            leastpade_addNum = pageFrame[i].pade_addNum;
        }
    }

    // If page in memory has been modified (dirtyBit = 1), then write page to disk
    if(pageFrame[leastpade_addIndex].dirtyBit == 1)
    {
        SM_FileHandle fh;
        openPageFile(bm->pageFile, &fh);
        writeBlock(pageFrame[leastpade_addIndex].pageNum, &fh, pageFrame[leastpade_addIndex].data);
        
        // Increase the write_count which records the number of writes done by the buffer manager.
        write_count++;
    }
    
    // Setting page frame's content to new page's content
    pageFrame[leastpade_addIndex].data = page->data;
    pageFrame[leastpade_addIndex].pageNum = page->pageNum;
    pageFrame[leastpade_addIndex].dirtyBit = page->dirtyBit;
    pageFrame[leastpade_addIndex].fixCount = page->fixCount;
    pageFrame[leastpade_addIndex].pade_addNum = page->pade_addNum;
}

// Defining CLOCK function
extern void CLOCK(BM_BufferPool *const bm, PageFrame *page)
{   
    //printf("CLOCK Started");
    PageFrame *pageFrame = (PageFrame *) bm->mgmtData;
    while(1)
    {
        clockPointer = (clockPointer % buffer_size == 0) ? 0 : clockPointer;

        if(pageFrame[clockPointer].pade_addNum == 0)
        {
            // If page in memory has been modified (dirtyBit = 1), then write page to disk
            if(pageFrame[clockPointer].dirtyBit == 1)
            {
                SM_FileHandle fh;
                openPageFile(bm->pageFile, &fh);
                writeBlock(pageFrame[clockPointer].pageNum, &fh, pageFrame[clockPointer].data);
                
                // Increase the write_count which records the number of writes done by the buffer manager.
                write_count++;
            }
            
            // Setting page frame's content to new page's content
            pageFrame[clockPointer].data = page->data;
            pageFrame[clockPointer].pageNum = page->pageNum;
            pageFrame[clockPointer].dirtyBit = page->dirtyBit;
            pageFrame[clockPointer].fixCount = page->fixCount;
            pageFrame[clockPointer].pade_addNum = page->pade_addNum;
            clockPointer++;
            break;  
        }
        else
        {
            // Incrementing clockPointer so that we can check the next page frame location.
            // We set pade_addNum = 0 so that this loop doesn't go into an infinite loop.
            pageFrame[clockPointer++].pade_addNum = 0;       
        }
    }
}

// ***** BUFFER POOL FUNCTIONS ***** //

/* 
   This function creates and initializes a buffer pool with numPages page frames.
   pageFileName stores the name of the page file whose pages are being cached in memory.
   strategy represents the page replacement strategy (FIFO, LRU, LFU, CLOCK) that will be used by this buffer pool
   stratData is used to pass parameters if any to the page replacement strategy
*/
extern RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, 
          const int numPages, ReplacementStrategy strategy, 
          void *stratData)
{
    bm->pageFile = (char *)pageFileName;
    bm->numPages = numPages;
    bm->strategy = strategy;

    // Reserver memory space = number of pages x space required for one page
    PageFrame *page = malloc(sizeof(PageFrame) * numPages);
    
    // buffer_size is the total number of pages in memory or the buffer pool.
    buffer_size = numPages;  
    int i;

    // Intilalizing all pages in buffer pool. The values of fields (variables) in the page is either NULL or 0
    for(i = 0; i < buffer_size; i++)
    {
        page[i].data = NULL;
        page[i].pageNum = -1;
        page[i].dirtyBit = 0;
        page[i].fixCount = 0;
        page[i].pade_addNum = 0; 
        page[i].refNum = 0;
    }

    bm->mgmtData = page;
    write_count = clockPointer = lfuPointer = 0;
    return RC_OK;
        
}

// Shutdown i.e. close the buffer pool, thereby removing all the pages from the memory and freeing up all resources and releasing some memory space.
extern RC shutdownBufferPool(BM_BufferPool *const bm)
{
    PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
    // Write all dirty pages (modified pages) back to disk
    forceFlushPool(bm);

    int i;  
    for(i = 0; i < buffer_size; i++)
    {
        // If fixCount != 0, it means that the contents of the page was modified by some client and has not been written back to disk.
        if(pageFrame[i].fixCount != 0)
        {
            return RC_PINNED_PAGES_IN_BUFFER;
        }
    }

    // Releasing space occupied by the page
    free(pageFrame);
    bm->mgmtData = NULL;
    return RC_OK;
}

// This function writes all the dirty pages (having fixCount = 0) to disk
extern RC forceFlushPool(BM_BufferPool *const bm)
{
    PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
    
    int i;
    // Store all dirty pages (modified pages) in memory to page file on disk    
    for(i = 0; i < buffer_size; i++)
    {
        if(pageFrame[i].fixCount == 0 && pageFrame[i].dirtyBit == 1)
        {
            SM_FileHandle fh;
            // Opening page file available on disk
            openPageFile(bm->pageFile, &fh);
            // Writing block of data to the page file on disk
            writeBlock(pageFrame[i].pageNum, &fh, pageFrame[i].data);
            // Mark the page not dirty.
            pageFrame[i].dirtyBit = 0;
            // Increase the write_count which records the number of writes done by the buffer manager.
            write_count++;
        }
    }   
    return RC_OK;
}


// ***** PAGE MANAGEMENT FUNCTIONS ***** //

// This function marks the page as dirty indicating that the data of the page has been modified by the client
extern RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page)
{
    PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
    
    int i;
    // Iterating through all the pages in the buffer pool
    for(i = 0; i < buffer_size; i++)
    {
        // If the current page is the page to be marked dirty, then set dirtyBit = 1 (page has been modified) for that page
        if(pageFrame[i].pageNum == page->pageNum)
        {
            pageFrame[i].dirtyBit = 1;
            return RC_OK;       
        }           
    }       
    return RC_ERROR;
}

// This function unpins a page from the memory i.e. removes a page from the memory
extern RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page)
{   
    PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
    
    int i;
    // Iterating through all the pages in the buffer pool
    for(i = 0; i < buffer_size; i++)
    {
        // If the current page is the page to be unpinned, then decrease fixCount (which means client has completed work on that page) and exit loop
        if(pageFrame[i].pageNum == page->pageNum)
        {
            pageFrame[i].fixCount--;
            break;      
        }       
    }
    return RC_OK;
}

// This function writes the contents of the modified pages back to the page file on disk
extern RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
    PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
    
    int i;
    // Iterating through all the pages in the buffer pool
    for(i = 0; i < buffer_size; i++)
    {
        // If the current page = page to be written to disk, then right the page to the disk using the storage manager functions
        if(pageFrame[i].pageNum == page->pageNum)
        {       
            SM_FileHandle fh;
            openPageFile(bm->pageFile, &fh);
            writeBlock(pageFrame[i].pageNum, &fh, pageFrame[i].data);
        
            // Mark page as undirty because the modified page has been written to disk
            pageFrame[i].dirtyBit = 0;
            
            // Increase the write_count which records the number of writes done by the buffer manager.
            write_count++;
        }
    }   
    return RC_OK;
}

// This function pins a page with page number pageNum i.e. adds the page with page number pageNum to the buffer pool.
// If the buffer pool is full, then it uses appropriate page replacement strategy to replace a page in memory with the new page being pinned. 
extern RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, 
        const PageNumber pageNum)
{
    PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
    
    // Checking if buffer pool is empty and this is the first page to be pinned
    if(pageFrame[0].pageNum == -1)
    {
        // Reading page from disk and initializing page frame's content in the buffer pool
        SM_FileHandle fh;
        openPageFile(bm->pageFile, &fh);
        pageFrame[0].data = (SM_PageHandle) malloc(PAGE_SIZE);
        ensureCapacity(pageNum,&fh);
        readBlock(pageNum, &fh, pageFrame[0].data);
        pageFrame[0].pageNum = pageNum;
        pageFrame[0].fixCount++;
        read_index = pade_add = 0;
        pageFrame[0].pade_addNum = pade_add;  
        pageFrame[0].refNum = 0;
        page->pageNum = pageNum;
        page->data = pageFrame[0].data;
        
        return RC_OK;       
    }
    else
    {   
        int i;
        bool isBufferFull = true;
        
        for(i = 0; i < buffer_size; i++)
        {
            if(pageFrame[i].pageNum != -1)
            {   
                // Checking if page is in memory
                if(pageFrame[i].pageNum == pageNum)
                {
                    // Increasing fixCount i.e. now there is one more client accessing this page
                    pageFrame[i].fixCount++;
                    isBufferFull = false;
                    pade_add++; // Incrementing pade_add (pade_add is used by LRU algorithm to determine the least recently used page)

                    if(bm->strategy == RS_LRU)
                        // LRU algorithm uses the value of pade_add to determine the least recently used page    
                        pageFrame[i].pade_addNum = pade_add;
                    else if(bm->strategy == RS_CLOCK)
                        // pade_addNum = 1 to indicate that this was the last page frame examined (added to the buffer pool)
                        pageFrame[i].pade_addNum = 1;
                    else if(bm->strategy == RS_LFU)
                        // Incrementing refNum to add one more to the count of number of times the page is used (referenced)
                        pageFrame[i].refNum++;
                    
                    page->pageNum = pageNum;
                    page->data = pageFrame[i].data;

                    clockPointer++;
                    break;
                }               
            } else {
                SM_FileHandle fh;
                openPageFile(bm->pageFile, &fh);
                pageFrame[i].data = (SM_PageHandle) malloc(PAGE_SIZE);
                readBlock(pageNum, &fh, pageFrame[i].data);
                pageFrame[i].pageNum = pageNum;
                pageFrame[i].fixCount = 1;
                pageFrame[i].refNum = 0;
                read_index++;    
                pade_add++; // Incrementing pade_add (pade_add is used by LRU algorithm to determine the least recently used page)

                if(bm->strategy == RS_LRU)
                    // LRU algorithm uses the value of pade_add to determine the least recently used page
                    pageFrame[i].pade_addNum = pade_add;              
                else if(bm->strategy == RS_CLOCK)
                    // pade_addNum = 1 to indicate that this was the last page frame examined (added to the buffer pool)
                    pageFrame[i].pade_addNum = 1;
                        
                page->pageNum = pageNum;
                page->data = pageFrame[i].data;
                
                isBufferFull = false;
                break;
            }
        }
        
        // If isBufferFull = true, then it means that the buffer is full and we must replace an existing page using page replacement strategy
        if(isBufferFull == true)
        {
            // Create a new page to store data read from the file.
            PageFrame *newPage = (PageFrame *) malloc(sizeof(PageFrame));       
            
            // Reading page from disk and initializing page frame's content in the buffer pool
            SM_FileHandle fh;
            openPageFile(bm->pageFile, &fh);
            newPage->data = (SM_PageHandle) malloc(PAGE_SIZE);
            readBlock(pageNum, &fh, newPage->data);
            newPage->pageNum = pageNum;
            newPage->dirtyBit = 0;      
            newPage->fixCount = 1;
            newPage->refNum = 0;
            read_index++;
            pade_add++;

            if(bm->strategy == RS_LRU)
                // LRU algorithm uses the value of pade_add to determine the least recently used page
                newPage->pade_addNum = pade_add;              
            else if(bm->strategy == RS_CLOCK)
                // pade_addNum = 1 to indicate that this was the last page frame examined (added to the buffer pool)
                newPage->pade_addNum = 1;

            page->pageNum = pageNum;
            page->data = newPage->data;         

            // Call appropriate algorithm's function depending on the page replacement strategy selected (passed through parameters)
            switch(bm->strategy)
            {           
                case RS_FIFO: // Using FIFO algorithm
                    FIFO(bm, newPage);
                    break;
                
                case RS_LRU: // Using LRU algorithm
                    LRU(bm, newPage);
                    break;
                
                case RS_CLOCK: // Using CLOCK algorithm
                    CLOCK(bm, newPage);
                    break;
                
                case RS_LFU: // Using LFU algorithm
                    LFU(bm, newPage);
                    break;
                
                case RS_LRU_K:
                    printf("\n LRU-k algorithm not implemented");
                    break;
                
                default:
                    printf("\nAlgorithm Not Implemented\n");
                    break;
            }
                        
        }       
        return RC_OK;
    }   
}


// ***** STATISTICS FUNCTIONS ***** //

extern PageNumber *getFrameContents(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return NULL; // Handle error for null pointers
    }

    // Allocate memory for the frame contents based on the buffer size
    PageNumber *frameContents = malloc(sizeof(PageNumber) * bm->numPages);
    if (frameContents == NULL) {
        return NULL; // Handle memory allocation failure
    }

    PageFrame *pageFrame = (PageFrame *)bm->mgmtData;

    // Initialize index variable
    int index = 0;

    // Use a for loop to iterate over each frame in the buffer pool
    for (index = 0; index < bm->numPages; index++) {
        // Assign the page number to frameContents, defaulting to NO_PAGE if the pageNum is -1
        frameContents[index] = (pageFrame[index].pageNum != -1) ? pageFrame[index].pageNum : NO_PAGE;
    }

    return frameContents; // Return the array of page numbers
}


// This function returns an array of bools, each element represents the dirtyBit of the respective page.
extern bool *getDirtyFlags (BM_BufferPool *const bm)
{
    bool *dirtyFlags = malloc(sizeof(bool) * buffer_size);
    PageFrame *pageFrame = (PageFrame *)bm->mgmtData;
    
    int i;
    // Iterating through all the pages in the buffer pool and setting dirtyFlags' value to TRUE if page is dirty else FALSE
    for(i = 0; i < buffer_size; i++)
    {
        dirtyFlags[i] = (pageFrame[i].dirtyBit == 1) ? true : false ;
    }   
    return dirtyFlags;
}

// This function returns an array of ints (of size numPages) where the ith element is the fix count of the page stored in the ith page frame.
extern int *getFixCounts (BM_BufferPool *const bm)
{
    int *fixCounts = malloc(sizeof(int) * buffer_size);
    PageFrame *pageFrame= (PageFrame *)bm->mgmtData;
    
    int i = 0;
    // Iterating through all the pages in the buffer pool and setting fixCounts' value to page's fixCount
    while(i < buffer_size)
    {
        fixCounts[i] = (pageFrame[i].fixCount != -1) ? pageFrame[i].fixCount : 0;
        i++;
    }   
    return fixCounts;
}

// This function returns the number of pages that have been read from disk since a buffer pool has been initialized.
extern int getNumReadIO (BM_BufferPool *const bm)
{
    // Adding one because with start read_index with 0.
    return (read_index + 1);
}

// This function returns the number of pages written to the page file since the buffer pool has been initialized.
extern int getNumWriteIO (BM_BufferPool *const bm)
{
    return write_count;
}

