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

// To make the given node the head of the list.
void updateHead(FrameList **list, PageFrameNode *updateNode) {
    // If the updateNode is NULL, the list is empty, or the node is already the
    // head, return immediately.
    if (updateNode == NULL || (*list)->head == NULL ||
        (*list)->head == updateNode) {
        return;
    }

    PageFrameNode *currentHead = (*list)->head;

    // If updateNode is the tail, update the tail pointer.
    if (updateNode == (*list)->tail) {
        (*list)->tail = updateNode->previousFrame;
        (*list)->tail->nextFrame = NULL;  // New tail's nextFrame is NULL
    }
    // If updateNode is in the middle of the list, adjust the links between its
    // neighbors.
    else {
        updateNode->previousFrame->nextFrame = updateNode->nextFrame;
        updateNode->nextFrame->previousFrame = updateNode->previousFrame;
    }

    // Now make updateNode the new head.
    updateNode->nextFrame = currentHead;
    currentHead->previousFrame = updateNode;
    updateNode->previousFrame = NULL;

    // Update the list's head to point to the updateNode.
    (*list)->head = updateNode;
}

/*
To find the node with given page number. It should be first checked by the
lookup array, and if available in memory, then to find the exact node.
*/
PageFrameNode *findNodeByPageNum(FrameList *list, const PageNumber pageNum) {
    // Use a for loop to traverse the list starting from the head.
    for (PageFrameNode *current = list->head; current != NULL;
         current = current->nextFrame) {
        // If the current node's page number matches, return it.
        if (current->pageNum == pageNum) {
            return current;
        }
    }

    // Return NULL if no matching node is found.
    return NULL;
}

PageFrameNode *pageInMemory(BM_BufferPool *const bm, BM_PageHandle *const page,
                            const PageNumber pageNum) {
    // Cast the management data to its actual type
    BMInfo *info = (BMInfo *)bm->mgmtData;

    // Check if the page number is mapped to a valid frame
    if (info->pageToFrame[pageNum] == NO_PAGE) {
        return NULL;  // Page is not in memory, return NULL
    }

    // Attempt to find the corresponding page frame node
    PageFrameNode *found = findNodeByPageNum(info->frames, pageNum);
    if (found == NULL) {
        return NULL;  // If not found, return NULL
    }

    // Set the page details for the client
    page->pageNum = pageNum;
    page->data = found->dataPoint;

    // Increment the fix count and mark the page as recently used
    found->fixCount++;
    found->rf = 1;

    return found;  // Return the found page frame node
}

RC updateNewFrame(BM_BufferPool *const bm, PageFrameNode *found,
                  BM_PageHandle *const page, const PageNumber pageNum) {
    // Initialize file handle and management info
    SM_FileHandle fh;
    BMInfo *info = (BMInfo *)bm->mgmtData;
    RC status;

    // Open the page file
    status = openPageFile((char *)(bm->pageFile), &fh);
    if (status != RC_OK) {
        return status;  // Return on error
    }

    if (found == NULL) {
        return RC_NON_EXISTING_PAGE_IN_FRAME;
    }

    // Check if the frame to be replaced is dirty; if so, write it back to disk
    if (found->dirty) {
        status = ensureCapacity(pageNum, &fh);
        if (status != RC_OK) {
            return status;  // Return on error
        }

        status = writeBlock(found->pageNum, &fh, found->dataPoint);
        if (status != RC_OK) {
            return status;  // Return on error
        }

        info->numWrite++;  // Increment the write count
    }

    // Update the pageToFrame mapping to mark the replaced page as NO_PAGE
    info->pageToFrame[found->pageNum] = NO_PAGE;

    // Ensure capacity and read the data into the new frame
    status = ensureCapacity(pageNum, &fh);
    if (status != RC_OK) {
        return status;  // Return on error
    }

    status = readBlock(pageNum, &fh, found->dataPoint);
    if (status != RC_OK) {
        return status;  // Return on error
    }

    // Provide the client with the data and details of the page
    page->pageNum = pageNum;
    page->data = found->dataPoint;

    info->numRead++;  // Increment the read count

    // Set all parameters of the new frame and update lookup arrays
    found->dirty = 0;          // Mark the frame as clean
    found->fixCount = 1;       // Set the fix count to 1
    found->pageNum = pageNum;  // Update the page number
    found->rf = 1;             // Mark the frame as recently used

    // Update the mapping arrays
    info->pageToFrame[found->pageNum] = found->frameNum;
    info->frameToPage[found->frameNum] = found->pageNum;

    // Close the file handle
    closePageFile(&fh);

    return RC_OK;  // Return success
}

/* PAGE REPLACEMENT STRATEGIES */

RC pinPage_FIFO(BM_BufferPool *const bm, BM_PageHandle *const page,
                const PageNumber pageNum) {
    // Initialize variables
    PageFrameNode *found;
    BMInfo *info = (BMInfo *)bm->mgmtData;

    // Check if the page is already in memory
    if ((found = pageInMemory(bm, page, pageNum)), found != NULL) {
        return RC_OK;  // Page is already in memory, return success
    }

    // If the page needs to be loaded
    // Check if there is space for a new page
    if (info->numFrames < bm->numPages) {
        // Find the first free frame from the head
        found = info->frames->head;
        for (int i = 0; i < info->numFrames; ++i) {
            found = found->nextFrame;  // Traverse to find the first free frame
        }

        // Increment the frame count as this frame will be filled now
        info->numFrames++;
        updateHead(&(info->frames),
                   found);  // Update the head to point to the new frame
    } else {
        // If all frames are filled, find the oldest frame with fix count 0
        found = info->frames->tail;
        while (found != NULL && found->fixCount != 0) {
            found = found->previousFrame;  // Traverse backward to find a frame
                                           // that can be replaced
        }

        // If no suitable frame is found, return error
        if (found == NULL) {
            return RC_NO_MORE_SPACE_IN_BUFFER;  // Buffer is full
        }

        updateHead(
            &(info->frames),
            found);  // Update the head to point to the frame being replaced
    }

    // Update the new frame with the page data
    RC status = updateNewFrame(bm, found, page, pageNum);
    if (status != RC_OK) {
        return status;  // Return any error encountered during update
    }

    return RC_OK;  // Return success
}

RC pinPage_LRU(BM_BufferPool *const bm, BM_PageHandle *const page,
               const PageNumber pageNum) {
    PageFrameNode *found;
    BMInfo *info = (BMInfo *)bm->mgmtData;

    // Check if the page is already in memory
    if ((found = pageInMemory(bm, page, pageNum)) != NULL) {
        // Update the head of the frame list as this frame is the most recently
        // used
        updateHead(&(info->frames), found);
        return RC_OK;  // Page is already in memory
    }

    // If the page needs to be loaded
    // Check if there is space for a new page
    if (info->numFrames < bm->numPages) {
        // Find the first free frame starting from the head
        found = info->frames->head;
        for (int i = 0; i < info->numFrames; ++i) {
            found = found->nextFrame;  // Traverse to find the first free frame
        }
        // Increment the frame count as this frame will be filled
        info->numFrames++;
    } else {
        // All frames are filled; look for a frame with fix count 0 starting
        // from the tail
        found = info->frames->tail;
        while (found != NULL && found->fixCount != 0) {
            found = found->previousFrame;  // Traverse backward to find a
                                           // suitable frame
        }
    }

    // Update the new frame with the page data
    RC status = updateNewFrame(bm, found, page, pageNum);
    if (status != RC_OK) {
        return status;  // Return any error encountered during update
    }

    // Update the head of the frame list as this frame is now the most recently
    // used
    updateHead(&(info->frames), found);

    return RC_OK;  // Return success
}

RC pinPage_LRU_K(BM_BufferPool *const bm, BM_PageHandle *const page,
                 const PageNumber pageNum) {
    PageFrameNode *found;
    BMInfo *info = (BMInfo *)bm->mgmtData;
    int K = (long)(info->stratData);
    int i;
    (info->countPinning)++;

    // Check if page already in memory. The pageToFrame array provides the fast lookup.
    if ((found = pageInMemory(bm, page, pageNum)) != NULL) {
        for (i = K - 1; i > 0; i--) {
            info->khist[found->pageNum][i] = info->khist[found->pageNum][i - 1];
        }
        
        info->khist[found->pageNum][0] = info->countPinning;

        return RC_OK;
    }
    /* If need to load the page*/

    /* If the frames in the memory are less than the total available frames,
     * find out the first free frame from the head. */
    if ((info->numFrames) < bm->numPages) {
        found = info->frames->head;

        int i = 0;
        while (i < info->numFrames) {
            found = found->nextFrame;
            ++i;
        }

        /*This frame will be filled up now, so increase the frame count*/
        (info->numFrames)++;
    } else {
        /* For new page, if all the frames are filled, then try to find a frame
         with fixed count 0 starting from the tail.*/
        PageFrameNode *current;
        int dist, max_dist = -1;

        current = info->frames->head;

        while (current != NULL) {
            if (current->fixCount == 0 &&
                info->khist[current->pageNum][K] != -1) {
                dist = info->countPinning - info->khist[current->pageNum][K];

                if (dist > max_dist) {
                    max_dist = dist;
                    found = current;
                }
            }
            current = current->nextFrame;
        }

        /* If reached to head, it means no frames with fixed count 0
         * available.*/
        if (max_dist == -1) {
            current = info->frames->head;

            while (current->fixCount != 0 && current != NULL) {
                dist = info->countPinning - info->khist[current->pageNum][0];
                if (dist > max_dist) {
                    max_dist = dist;
                    found = current;
                }
                current = current->nextFrame;
            }

            /* If reached to head, it means no frames with fixed count 0
             * available.*/
            if (max_dist == -1) {
                return RC_NO_MORE_SPACE_IN_BUFFER;
            }
        }
    }

    RC status;

    if ((status = updateNewFrame(bm, found, page, pageNum)) != RC_OK) {
        return status;
    }

    for (i = K - 1; i > 0; i--) {
        info->khist[found->pageNum][i] = info->khist[found->pageNum][i - 1];
    }
    info->khist[found->pageNum][0] = info->countPinning;

    return RC_OK;
}

RC pinPage_LFU(BM_BufferPool *const bm, BM_PageHandle *const page,
               const PageNumber pageNum) {
    PageFrameNode *found = 0;
    BMInfo *info = (BMInfo *)bm->mgmtData;  // Get buffer manager info
    page->data =
        (SM_PageHandle)malloc(PAGE_SIZE);  // Allocate memory for the page data

    // Check if the requested page is already in memory
    if ((info->pageToFrame)[pageNum] != -1) {
        // Page found in memory; retrieve the corresponding frame
        PageFrameNode *found = findNodeByPageNum(info->frames, pageNum);

        // Update page details for the client
        page->pageNum = pageNum;
        page->data = found->dataPoint;

        // Update frame metadata: increase fix count and frequency
        found->fixCount++;
        found->pageFrequency++;
        found->rf = 1;

        return RC_OK;  // Successful retrieval
    }

    // If the page needs to be loaded and there's available space in memory
    if (info->numFrames < bm->numPages) {
        // Find the first free frame from the head of the frame list
        PageFrameNode *found = info->frames->head;
        while (found->nextFrame != NULL) {
            found = found->nextFrame;  // Traverse to find a free frame
        }

        // Increase frame count since we're using this frame now
        info->numFrames++;
        updateHead(&(info->frames), found);  // Update the head pointer
    } else {
        // If all frames are occupied, find the least frequently used frame
        PageFrameNode *found = info->frames->tail;

        // Traverse back to find the oldest frame with fix count 0
        while (found != NULL &&
               (found->previousFrame == NULL ||
                found->previousFrame->pageFrequency < found->pageFrequency)) {
            found = found->previousFrame;
        }

        // Check if a frame with fix count 0 was found
        while (found != NULL && found->fixCount != 0) {
            found = found->previousFrame;  // Move to the previous frame
        }

        // If no suitable frame is available, return error
        if (found == NULL) {
            return RC_NO_MORE_SPACE_IN_BUFFER;
        }

        updateHead(&(info->frames), found);  // Update the head pointer
    }

    // Prepare to load the page from the disk
    SM_FileHandle fh;
    if (openPageFile(bm->pageFile, &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;  // Error opening the file
    }

    // Write back the dirty frame to disk if necessary
    if (found->dirty == 1) {
        writeBlock(found->pageNum, &fh, found->dataPoint);
        info->numWrite++;  // Increment the write count
    }

    // Update the lookup for pageToFrame and set the replaced page to NO_PAGE
    (info->pageToFrame)[found->pageNum] = NO_PAGE;

    // Allocate new space for the incoming page
    found->dataPoint = (SM_PageHandle)malloc(PAGE_SIZE);
    ensureCapacity(pageNum, &fh);  // Ensure there is capacity for the page
    readBlock(pageNum, &fh,
              found->dataPoint);  // Read the new page into the frame

    // Update the page details for the client
    page->pageNum = pageNum;
    page->data = found->dataPoint;

    info->numRead++;  // Increment the read count

    // Initialize frame metadata for the new page
    found->dirty = 0;
    found->fixCount = 1;
    found->pageNum = pageNum;
    found->rf = 1;
    found->pageFrequency = 1;

    // Update the mapping arrays
    (info->pageToFrame)[found->pageNum] = found->frameNum;
    (info->frameToPage)[found->frameNum] = found->pageNum;

    return RC_OK;  // Successful pinning of the page
}

RC pinPage_CLOCK(BM_BufferPool *const bm, BM_PageHandle *const page,
                 const PageNumber pageNum) {
    BMInfo *info = (BMInfo *)bm->mgmtData;
    PageFrameNode *found = pageInMemory(bm, page, pageNum);

    // Check if the page is already loaded in memory
    if (found != NULL) {
        return RC_OK;  // Page is already in memory
    }

    // If the page is not in memory, search for a free frame
    PageFrameNode *currentFrame = info->frames->head;

    // Look for the first frame with a reference count of 0
    while (currentFrame != NULL) {
        // If the current frame has a reference count of 1, reset it to 0
        if (currentFrame->rf == 1) {
            currentFrame->rf = 0;
        } else {
            // Found a frame with rf = 0
            found = currentFrame;
            break;
        }
        currentFrame = currentFrame->nextFrame;  // Move to the next frame
    }

    // If no free frame is found, return an error
    if (found == NULL) {
        return RC_NO_MORE_SPACE_IN_BUFFER;  // No more space in buffer
    }

    // Attempt to update the found frame with the new page
    RC status = updateNewFrame(bm, found, page, pageNum);

    // Check if the update was successful
    if (status != RC_OK) {
        return status;  // Return the error code if update fails
    }

    return RC_OK;  // Successfully pinned the page
}



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
    // Check for a valid buffer pool
    if (!bm || bm->numPages <= 0) {
        return RC_INVALID_BM;  // Invalid buffer pool
    }

    // Access management information
    BMInfo *info = (BMInfo *)bm->mgmtData;

    // Locate the page to be marked as dirty
    PageFrameNode *found = findNodeByPageNum(info->frames, page->pageNum);
    if (found != NULL) {
        // Mark the page as dirty
        found->dirty = true;

        return RC_OK;  // Successfully marked as dirty
    }

    return RC_NON_EXISTING_PAGE_IN_FRAME;  // Page not found in buffer
}


RC unpinPage(BM_BufferPool *const bm, BM_PageHandle *const page) {
// Check for a valid buffer pool
    if (!bm || bm->numPages <= 0) {
        return RC_INVALID_BM;  // Invalid buffer pool
    }

    // Access management information
    BMInfo *info = (BMInfo *)bm->mgmtData;

    // Locate the page to be unpinned
    PageFrameNode *found = findNodeByPageNum(info->frames, page->pageNum);
    if (found == NULL) {
        return RC_NON_EXISTING_PAGE_IN_FRAME;  // Page not found in buffer
    }

    // Decrease the fix count for the page if it's greater than 0
    if (found->fixCount > 0) {
        found->fixCount--;
    } else {
        return RC_NON_EXISTING_PAGE_IN_FRAME;  // No fix count to decrement
    }

    return RC_OK;  // Successfully unpinned
    
}

RC forcePage(BM_BufferPool *const bm, BM_PageHandle *const page) {
    // Check if the buffer pool is valid and contains pages
    if (!bm || bm->numPages <= 0) {
        return RC_INVALID_BM;  // Return error for invalid buffer pool
    }

    // Retrieve buffer management information
    BMInfo *info = (BMInfo *)bm->mgmtData;
    PageFrameNode *found;  // Pointer to locate the page frame
    SM_FileHandle fh;      // File handle for the page file

    // Attempt to open the page file; if it fails, return an error
    if (openPageFile((char *)(bm->pageFile), &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;  // Error: page file not found
    }

    // Locate the page frame by page number
    found = findNodeByPageNum(info->frames, page->pageNum);
    if (found == NULL) {
        closePageFile(&fh);  // Close the page file before returning
        return RC_NON_EXISTING_PAGE_IN_FRAME;  // Error: page not in frame
    }

    // Write the current page data back to the page file on disk
    if (writeBlock(found->pageNum, &fh, found->dataPoint) != RC_OK) {
        closePageFile(&fh);      // Close the page file before returning
        return RC_WRITE_FAILED;  // Error: writing to the file failed
    }

    // Increment the write count in the buffer management info
    info->numWrite++;

    // Close the page file after operations are complete
    closePageFile(&fh);

    return RC_OK;  // Successful operation
}


RC pinPage(BM_BufferPool *const bm, BM_PageHandle *const page,
           const PageNumber pageNum) {
            // Check if the buffer pool is valid and contains pages
    if (!bm || bm->numPages <= 0) {
        return RC_INVALID_BM;  // Error: invalid buffer pool
    }

    // Validate the requested page number
    if (pageNum < 0) {
        return RC_READ_NON_EXISTING_PAGE;  // Error: non-existing page
    }

    // Check the page replacement strategy using if-else statements
    if (bm->strategy == RS_FIFO) {
        return pinPage_FIFO(bm, page, pageNum);  // FIFO strategy
    } else if (bm->strategy == RS_LRU) {
        return pinPage_LRU(bm, page, pageNum);  // LRU strategy
    } else if (bm->strategy == RS_CLOCK) {
        return pinPage_CLOCK(bm, page, pageNum);  // CLOCK strategy
    } else if (bm->strategy == RS_LFU) {
        return pinPage_LFU(bm, page, pageNum);  // LFU strategy
    } else if (bm->strategy == RS_LRU_K) {
        return pinPage_LRU_K(bm, page, pageNum);  // LRU-K strategy
    } else {
        return RC_UNKNOWN_STRATEGY;  // Error: unknown replacement strategy
    }

    // Although unreachable, returning success for consistency
    return RC_OK;
}

/* STATISTICS FUNCTIONS */

PageNumber *getFrameContents(BM_BufferPool *const bm) {
    // Check if the buffer pool is valid
    if (bm == NULL || bm->mgmtData == NULL) {
        return NULL;  // Return NULL if the buffer pool or management data is
                      // invalid
    }

    // Cast the management data to BMInfo and retrieve the frame-to-page mapping
    BMInfo *info = (BMInfo *)bm->mgmtData;
    return info
        ->frameToPage;  // Return the pointer to the frame-to-page mapping
}

bool *getDirtyFlags(BM_BufferPool *const bm) {
    // Check if the buffer pool is valid
    if (bm == NULL || bm->mgmtData == NULL) {
        return NULL;  // Return NULL if the buffer pool or management data is
                      // invalid
    }

    // Cast the management data to BMInfo for easier access
    BMInfo *info = (BMInfo *)bm->mgmtData;

    // Iterate through frames list from the head of the list
    for (PageFrameNode *cur = info->frames->head; cur != NULL;
         cur = cur->nextFrame) {
        info->dirtyFlags[cur->frameNum] =
            cur->dirty;  // Set the dirty flag for the current frame
    }

    // Return the updated dirtyFlags array
    return info->dirtyFlags;
}

int *getFixCounts(BM_BufferPool *const bm) {
    // Check if the buffer pool is valid
    if (bm == NULL || bm->mgmtData == NULL) {
        return NULL;  // Return NULL if the buffer pool or management data is
                      // invalid
    }

    // Cast the management data to BMInfo for easier access
    BMInfo *info = (BMInfo *)bm->mgmtData;
    PageFrameNode *cur =
        info->frames->head;  // Start at the head of the frame list

    // Use a for loop to iterate through the list of frames
    for (; cur != NULL; cur = cur->nextFrame) {
        info->fixedCounts[cur->frameNum] =
            cur->fixCount;  // Update the fix count for the current frame
    }

    // Return the updated fixedCounts array
    return info->fixedCounts;
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
