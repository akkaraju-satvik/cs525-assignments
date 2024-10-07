#include "buffer_mgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dberror.h"
#include "storage_mgr.h"
#define MAX_PAGES 20000
#define MAX_FRAMES 200
#define MAX_K 10

// A frame node consisting one page/frame of a buffer pool
typedef struct PageFrameNode {
    int pageNum;        // the pageNum of page in the pageFile
    int frameNum;       // the number of the frame in the frame list
    int dirty;          // dirty = 1, not dirty = 0
    int fixCount;       // fixCount of the page based on the pinning/un-pinning
                        // request
    int rf;             // reference bit per node to be used by clock
    int pageFrequency;  // frequency of the page per client request for LFU
    char *dataPoint;    // actual data pointed by the frameNode.
    struct PageFrameNode *nextFrame;
    struct PageFrameNode *previousFrame;

} PageFrameNode;

// A frame list with a pointer to head and tail node of type PageFrameNode
typedef struct FrameList {
    PageFrameNode *head;  // head node of the frame list. Should add
                          // new/updated/recently used node to head
    PageFrameNode *tail;  // tail node of the frame list. Should be the
                          // first/start to remove as per strategy

} FrameList;

// Required data per buffer pool. Will be attached to BM_BufferPool->mgmtData
typedef struct BMInfo {
    int numFrames;     // filled number of frames in the frame list
    int numRead;       // number of reads done on the buffer pool
    int numWrite;      // number of writes done on the buffer pool
    int countPinning;  // total number of pinning done for the bm
    void *stratData;
    int pageToFrame[MAX_PAGES];  // an array from pageNumber to frameNumber. The
                                 // size should be same as the size of the
                                 // pageFile.
    int frameToPage[MAX_FRAMES];  // an array from frameNumber to pageNumber.
                                  // The size should be same as the size of the
                                  // frame list.
    int pageToFrequency[MAX_PAGES];  // an array mapping pageNumber to
                                     // pageFrequency. The size should be same
                                     // as the size of the pageFile.
    bool dirtyFlags[MAX_FRAMES];     // dirtyFlags of all the frames.
    int fixedCounts[MAX_FRAMES];     // fixed count of all the frames.
    int khist[MAX_PAGES][MAX_K];  // history of reference of each page in memory
    FrameList *frames;  // a pointer to the frame list of current buffer pool

} BMInfo;

/* UTILITY FUNCTIONS */

// To create a new node for frame list
PageFrameNode *newNode() {
    PageFrameNode *node = malloc(sizeof(PageFrameNode));
    node->pageNum = NO_PAGE;
    node->frameNum = 0;
    node->dirty = 0;
    node->fixCount = 0;
    node->rf = 0;
    node->dataPoint = calloc(PAGE_SIZE, sizeof(char));
    node->nextFrame = NULL;
    node->previousFrame = NULL;
    node->pageFrequency = 0;

    return node;
}


/* PAGE REPLACEMENT STRATEGIES */



/*Buffer Pool Functions*/

RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,
                  const int numPages, ReplacementStrategy strategy,
                  void *stratData) {
    // Check for invalid number of pages
    if (numPages <= 0) {
        return RC_INVALID_BM;  // Invalid buffer pool size
    }

    // Open the page file and check for success
    SM_FileHandle fh;
    if (openPageFile((char *)pageFileName, &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;  // File not found
    }

    // Allocate and initialize management information
    BMInfo *info = malloc(sizeof(BMInfo));

    info->numFrames = 0;
    info->numRead = 0;
    info->numWrite = 0;
    info->stratData = stratData;
    info->countPinning = 0;

    // Initialize lookup arrays
    memset(info->frameToPage, NO_PAGE, MAX_FRAMES * sizeof(int));
    memset(info->pageToFrame, NO_PAGE, MAX_PAGES * sizeof(int));
    memset(info->dirtyFlags, NO_PAGE, MAX_FRAMES * sizeof(bool));
    memset(info->fixedCounts, NO_PAGE, MAX_FRAMES * sizeof(int));
    memset(info->khist, -1, sizeof(info->khist));  // Correct usage of sizeof
    memset(info->pageToFrequency, 0, MAX_PAGES * sizeof(int));

    // Create the linked list of frames
    info->frames = malloc(sizeof(FrameList));

    // Initialize the first frame node
    info->frames->head = info->frames->tail = newNode();

    // Use a while loop to create additional frames in the linked list
    int i = 1;
    while (i < numPages) {
        PageFrameNode *newFrame = newNode();
        info->frames->tail->nextFrame = newFrame;
        newFrame->previousFrame = info->frames->tail;
        info->frames->tail = newFrame;
        info->frames->tail->frameNum = i;
        i++;  // Increment the frame count
    }

    // Set up buffer pool properties
    bm->numPages = numPages;
    bm->pageFile = (char *)pageFileName;
    bm->strategy = strategy;
    bm->mgmtData = info;

    // Close the page file
    closePageFile(&fh);

    return RC_OK;  // Successful initialization
}

RC shutdownBufferPool(BM_BufferPool *const bm) {
    // Check for a valid buffer pool
    if (!bm || bm->numPages <= 0) {
        return RC_INVALID_BM;  // Invalid buffer pool
    }

    RC status;

    // Force flush dirty pages to disk and return error if the operation fails
    if ((status = forceFlushPool(bm)), status != RC_OK) {
        return status;
    }

    // Access management information
    BMInfo *info = (BMInfo *)bm->mgmtData;
    PageFrameNode *current = info->frames->head;

    // Free all the frames in the linked list
    for (PageFrameNode *nextFrame; current != NULL; current = nextFrame) {
        nextFrame = current->nextFrame;  // Save the next frame
        free(current->dataPoint);        // Free the data point
        free(current);                   // Free the current frame
    }

    free(info->frames);  // Free the frame list structure
    free(info);          // Free management information structure

    // Reset buffer pool properties
    bm->numPages = 0;
    bm->mgmtData = NULL;  // Nullify mgmtData to prevent any unhandled pointers

    return RC_OK;
}

RC forceFlushPool(BM_BufferPool *const bm) {
    // Check for a valid buffer pool
    if (!bm || bm->numPages <= 0) {
        return RC_INVALID_BM;  // Invalid buffer pool
    }

    // Access management information
    BMInfo *info = (BMInfo *)bm->mgmtData;
    PageFrameNode *current = info->frames->head;

    // Open the page file
    SM_FileHandle fh;
    if (openPageFile((char *)(bm->pageFile), &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;  // File not found
    }

    // Flush dirty frames to disk using a for loop
    for (; current != NULL; current = current->nextFrame) {
        if (current->dirty) {
            // Write the dirty page to the disk
            if (writeBlock(current->pageNum, &fh, current->dataPoint) !=
                RC_OK) {
                closePageFile(&fh);      // Close the file before returning
                return RC_WRITE_FAILED;  // Writing failed
            }
            current->dirty = false;  // Mark the frame as clean
            (info->numWrite)++;      // Increment the write count
        }
    }

    // Close the page file
    closePageFile(&fh);

    return RC_OK;  // Successful flush
}

/* PAGE MANAGEMENT FUNCTIONS */

RC markDirty(BM_BufferPool *const bm, BM_PageHandle *const page) {
}

RC unpinPage(BM_BufferPool *const bm, BM_PageHandle *const page) {
}

RC forcePage(BM_BufferPool *const bm, BM_PageHandle *const page) {
}

RC pinPage(BM_BufferPool *const bm, BM_PageHandle *const page,
           const PageNumber pageNum) {
}

/* STATISTICS FUNCTIONS */

PageNumber *getFrameContents(BM_BufferPool *const bm) {
}

bool *getDirtyFlags(BM_BufferPool *const bm) {
}

int *getFixCounts(BM_BufferPool *const bm) {
}

int getNumReadIO(BM_BufferPool *const bm) {
    // Check if the buffer pool is valid
    if (bm == NULL || bm->mgmtData == NULL) {
        return -1;  // Return -1 or an appropriate error code if the buffer pool
                    // is invalid
    }

    // Cast the management data to BMInfo for easier access and return numRead
    BMInfo *info = (BMInfo *)bm->mgmtData;
    return info->numRead;  // Return the number of read I/O operations
}

int getNumWriteIO(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return -1;  // Return -1 or an appropriate error code if the buffer pool
                    // is invalid
    }
    /*returns the value of numWrite*/
    return ((BMInfo *)bm->mgmtData)->numWrite;
}
