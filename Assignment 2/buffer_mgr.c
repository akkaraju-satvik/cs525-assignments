#include "buffer_mgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dberror.h"
#include "storage_mgr.h"
#define MAX_PAGES 20000
#define MAX_FRAMES 200
#define MAX_K 10

typedef struct NodeData {
    int pageNum;
    int frameNum;
    int dirty;
    int fixCount;
    int referenceBit;
    int pageFrequency;
    char *dataPoint;
} NodeData;

typedef struct PageFrameNode {
    NodeData data;
    struct PageFrameNode *nextFrame;
    struct PageFrameNode *previousFrame;

} PageFrameNode;

typedef struct FrameList {
    PageFrameNode *head;
    PageFrameNode *tail;
} FrameList;

typedef struct BMStats {
    int numRead;
    int numWrite;
    int numPins;
} BMStats;

typedef struct BMInfo {
    BMStats stats;
    int numOfFrames;
    void *stratData;
    int pageToFrame[MAX_PAGES];
    int frameToPage[MAX_FRAMES];
    int pageToFrequency[MAX_PAGES];
    bool dirtyFlags[MAX_FRAMES];
    int fixedCounts[MAX_FRAMES];
    int khist[MAX_PAGES][MAX_K];
    FrameList *frames;

} BMInfo;

/* UTILITY FUNCTIONS */

// To create a new node for frame list
PageFrameNode *newNode() {
    PageFrameNode *node = malloc(sizeof(PageFrameNode));
    node->data.pageNum = NO_PAGE;
    node->data.frameNum = 0;
    node->data.dirty = 0;
    node->data.fixCount = 0;
    node->data.referenceBit = 0;
    node->data.dataPoint = calloc(PAGE_SIZE, sizeof(char));
    node->data.pageFrequency = 0;
    node->nextFrame = NULL;
    node->previousFrame = NULL;

    return node;
}

void updateCursor(FrameList **list, PageFrameNode *updateNode) {
    if (!updateNode || !(*list)->head || (*list)->head == updateNode) {
        return;
    }

    PageFrameNode *currentHead = (*list)->head;

    if (updateNode == (*list)->tail) {
        (*list)->tail = updateNode->previousFrame;
        if ((*list)->tail) {
            (*list)->tail->nextFrame = NULL;
        }
    } else {
        if (updateNode->previousFrame) {
            updateNode->previousFrame->nextFrame = updateNode->nextFrame;
        }
        if (updateNode->nextFrame) {
            updateNode->nextFrame->previousFrame = updateNode->previousFrame;
        }
    }

    updateNode->nextFrame = currentHead;
    currentHead->previousFrame = updateNode;
    updateNode->previousFrame = NULL;

    (*list)->head = updateNode;
}

/*
To find the node with given page number. It should be first checked by the
lookup array, and if available in memory, then to find the exact node.
*/

PageFrameNode *findNodeByPageNum(FrameList *list, const PageNumber pageNum) {
    for (PageFrameNode *current = list->head; current != NULL;
         current = current->nextFrame) {
        // If the current node's page number matches, return it.
        if (current->data.pageNum == pageNum) {
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
        return NULL;
    }
    // Attempt to find the corresponding page frame node
    PageFrameNode *f = findNodeByPageNum(info->frames, pageNum);
    if (!f) {
        return NULL;
    }
    // Set the page details for the client
    page->pageNum = pageNum;
    page->data = f->data.dataPoint;
    // Increment the fix count and mark the page as recently used
    f->data.fixCount++;
    f->data.referenceBit = 1;

    return f;
}

RC updateNewFrame(BM_BufferPool *const bm, PageFrameNode *f,
                  BM_PageHandle *const page, const PageNumber pageNum) {
    SM_FileHandle fh;
    BMInfo *info = (BMInfo *)bm->mgmtData;
    RC s;

    s = openPageFile((char *)(bm->pageFile), &fh);
    if (s != RC_OK) {
        return s;
    }

    if (!f) {
        return RC_NON_EXISTING_PAGE_IN_FRAME;
    }
    // Check if the frame to be replaced is dirty; if so, write it back to disk
    if (f->data.dirty) {
        if (s = ensureCapacity(pageNum, &fh), s != RC_OK) {
            return s;
        }
        if (s =
                writeBlock(f->data.pageNum, &fh, f->data.dataPoint),
            s != RC_OK) {
            return s;
        }
        info->stats.numWrite++;
    }
    // Update the pageToFrame mapping to mark the replaced page as NO_PAGE
    info->pageToFrame[f->data.pageNum] = NO_PAGE;
    // Ensure capacity and read the data into the new frame
    s = ensureCapacity(pageNum, &fh);
    if (s != RC_OK) {
        return s;
    }

    s = readBlock(pageNum, &fh, f->data.dataPoint);
    if (s != RC_OK) {
        return s;
    }
    // Provide the client with the data and details of the page
    page->pageNum = pageNum;
    page->data = f->data.dataPoint;

    info->stats.numRead++;
    
    // Set all parameters of the new frame and update lookup arrays
    f->data.dirty = 0;
    f->data.fixCount = 1;
    f->data.pageNum = pageNum;
    f->data.referenceBit = 1;

    info->pageToFrame[f->data.pageNum] = f->data.frameNum;
    info->frameToPage[f->data.frameNum] = f->data.pageNum;
    closePageFile(&fh);
      // Close the file handle
    return RC_OK;
}

/* PAGE REPLACEMENT STRATEGIES */

RC pinPage_FIFO(BM_BufferPool *const bm, BM_PageHandle *const page,
                const PageNumber pageNum) {
    PageFrameNode *f;
    BMInfo *info = (BMInfo *)bm->mgmtData;

   f = pageInMemory(bm, page, pageNum);
    if (f != NULL) {
        return RC_OK;
    }
     // If the page needs to be loaded
    // Check if there is space for a new page
    if (info->numOfFrames < bm->numPages) {
        f = info->frames->head;
      for (int i = 0; i < info->numOfFrames; i++) {
    f = f->nextFrame;
}


        info->numOfFrames++;
        updateCursor(&(info->frames), f);
    } else {
        f = info->frames->tail;
        // Traverse backward to find a frame that can be replaced
        for (; f != NULL && f->data.fixCount != 0; f = f->previousFrame) {
        }
         // If no suitable frame is found, return error
        if (f == NULL) {
            return RC_NO_MORE_SPACE_IN_BUFFER;
        }

        updateCursor(&(info->frames), f);
    }

    RC s = updateNewFrame(bm, f, page, pageNum);
    if (s != RC_OK) {
        return s;
    }

    return RC_OK;
}


RC pinPage_LRU(BM_BufferPool *const bm, BM_PageHandle *const page,
               const PageNumber pageNum) {
    PageFrameNode *f;
    BMInfo *info = (BMInfo *)bm->mgmtData;

    RC s;
      // Update the head of the frame list as this frame is the most recently used
    if ((f = pageInMemory(bm, page, pageNum)), f != NULL) {
        updateCursor(&(info->frames), f);
        s = RC_OK;
        return s;
    }
    // If the page needs to be loaded
    // Check if there is space for a new page
    if (info->numOfFrames < bm->numPages) {
        f = info->frames->head;
        for (int i = 0; i < info->numOfFrames; ++i) {
            f = f->nextFrame; // Traverse to find the first free frame
        }
         // Increment the frame count as this frame will be filled
        ++info->numOfFrames;
    } else {
        // All frames are filled; look for a frame with fix count 0 starting
        // from the tail
        f = info->frames->tail;
        while (f != NULL && f->data.fixCount != 0) {
            f = f->previousFrame;
        }
    }
    // Update the head of the frame list as this frame is now the most recently
    // used
    s= updateNewFrame(bm, f, page, pageNum);
if (s!= RC_OK) {
    return s;
}


    updateCursor(&(info->frames), f);

    return RC_OK;
}

RC pinPage_LRU_K(BM_BufferPool *const bm, BM_PageHandle *const page,
                 const PageNumber pageNum) {
    PageFrameNode *f;
    BMInfo *info = (BMInfo *)bm->mgmtData;
    int K = (long)(info->stratData);
    int i;

    (info->stats.numPins)++;
    // Check if page already in memory. The pageToFrame array provides the fast lookup.
    if ((f = pageInMemory(bm, page, pageNum)) != NULL) {
        i = K - 1;
        while (i > 0) {
            info->khist[f->data.pageNum][i] =
                info->khist[f->data.pageNum][i - 1];
            i--;
        }

        info->khist[f->data.pageNum][0] = info->stats.numPins;

        return RC_OK;
    }
     /* If need to load the page*/

    /* If the frames in the memory are less than the total available frames,
     * find out the first free frame from the head. */
    if ((info->numOfFrames) < bm->numPages) {
        f = info->frames->head;



        for (i = 0; i < info->numOfFrames; ++i) {
            f = f->nextFrame;
        }

        (info->numOfFrames)++;
    } else {
         /* For new page, if all the frames are filled, then try to find a frame
         with fixed count 0 starting from the tail.*/
        PageFrameNode *current;
        int dist, max_dist = -1;
    
        current = info->frames->head;

        for (; current != NULL; ) {

             if (dist > max_dist) {
                    max_dist = dist;
                    f = current;
                }
            if (current->data.fixCount == 0 &&
                info->khist[current->data.pageNum][K] != -1) {
                dist =info->stats.numPins - info->khist[current->data.pageNum][K];

               
            }
            current = current->nextFrame;
        }
         /* If reached to head, it means no frames with fixed count 0
         * available.*/
         if (max_dist == -1) {
                return RC_NO_MORE_SPACE_IN_BUFFER;
            }

        if (max_dist == -1) {
            current = info->frames->head;
            for (; current->data.fixCount != 0 && current != NULL; ) {
                dist =info->stats.numPins - info->khist[current->data.pageNum][0];
                if (dist > max_dist) {
                    max_dist = dist;
                    f = current;
                }
                current = current->nextFrame;
            }

           
        }
    }

    RC s;

    s= updateNewFrame(bm, f, page, pageNum);
if (s!= RC_OK) {
    return s;
}

    i = K - 1;
    while (i > 0) {
        info->khist[f->data.pageNum][i] =
            info->khist[f->data.pageNum][i - 1];
        i--;
    }
    info->khist[f->data.pageNum][0] = info->stats.numPins;

    return RC_OK;
}

/*Buffer Pool Functions*/

RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,
                  const int numPages, ReplacementStrategy strategy,
                  void *stratData) {
     // Check for invalid number of pages
    if (numPages <= 0) {
        return RC_INVALID_BM;
    }
 // Open the page file and check for success
    SM_FileHandle fh;
    if (openPageFile((char *)pageFileName, &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;  // File not f
    }
    // Allocate and initialize management information
    BMInfo *info = malloc(sizeof(BMInfo));

    info->stats.numRead = 0;
    info->stats.numWrite = 0;
    info->stratData = stratData;
    info->stats.numPins = 0;
    // Initialize lookup arrays
    memset(info->frameToPage, NO_PAGE, MAX_FRAMES * 4);
    memset(info->pageToFrame, NO_PAGE, MAX_PAGES * 4);
    info->numOfFrames = 0;
    memset(info->dirtyFlags, NO_PAGE, MAX_FRAMES * 1);
    memset(info->fixedCounts, NO_PAGE, MAX_FRAMES * 4);
    memset(info->khist, -1, sizeof(info->khist));  // Correct usage of sizeof
    memset(info->pageToFrequency, 0, MAX_PAGES * sizeof(int));
    // Create the linked list of frames
    int SIZE=sizeof(FrameList);
    info->frames = malloc(SIZE);
     // Initialize the first frame node
    info->frames->head = info->frames->tail = newNode();
    // Use a while loop to create additional frames in the linked list
    int i = 1;
    while (i < numPages) {
        PageFrameNode *newFrame = newNode();
        info->frames->tail->nextFrame = newFrame;
        newFrame->previousFrame = info->frames->tail;
        info->frames->tail = newFrame;
        info->frames->tail->data.frameNum = i;
        i++;
    }
 // Set up buffer pool properties
   bm->mgmtData = info;
bm->strategy = strategy;
bm->pageFile = (char *)pageFileName;
bm->numPages = numPages;

closePageFile(&fh);

return RC_OK;

}

RC shutdownBufferPool(BM_BufferPool *const bm) {
    // Check for a valid buffer pool
      if (!(bm && bm->numPages > 0)) {
    return RC_INVALID_BM;
}

    RC s;
    // Force flush dirty pages to disk and return error if the operation fails
    if ((s = forceFlushPool(bm))&& s != RC_OK) {
        return s;
    }
    // Access management information
    BMInfo *info = (BMInfo *)bm->mgmtData;
    PageFrameNode *current = info->frames->head;
  // Free all the frames in the linked list
    for (PageFrameNode *nextFrame; current != NULL; current = nextFrame) {
        nextFrame = current->nextFrame;
        free(current->data.dataPoint);
        free(current);
    }

    free(info->frames);
    free(info);

    bm->numPages = 0;
    bm->mgmtData = NULL;

    return RC_OK;
}

RC forceFlushPool(BM_BufferPool *const bm) {
   if (!(bm && bm->numPages > 0)) {
    return RC_INVALID_BM;
}
// Access management information

    BMInfo *info = (BMInfo *)bm->mgmtData;
    PageFrameNode *current = info->frames->head;

    SM_FileHandle fh;
    int result = openPageFile(bm->pageFile, &fh);
    if (result != RC_OK) {
    return result;
    }

// Flush dirty frames to disk using a for loop
    for (; current != NULL; current = current->nextFrame) {
        if (current->data.dirty) {
            // Write the dirty page to the disk
            if (writeBlock(current->data.pageNum, &fh,
                           current->data.dataPoint) != RC_OK) {
                closePageFile(&fh);
                return RC_WRITE_FAILED;
            }
            current->data.dirty = false;
            (info->stats.numWrite)++;
        }
    }

    closePageFile(&fh);

    return RC_OK;
}

/* PAGE MANAGEMENT FUNCTIONS */

RC markDirty(BM_BufferPool *const bm, BM_PageHandle *const page) {
    // Check for a valid buffer pool
    if (!(bm && bm->numPages > 0)) {
    return RC_INVALID_BM;
}
// Access management information
    BMInfo *info = (BMInfo *)bm->mgmtData;
    // Locate the page to be marked as dirty
    PageFrameNode *f = findNodeByPageNum(info->frames, page->pageNum);
    if (f != NULL) {
        f->data.dirty = true;

        return RC_OK;
    }

    return RC_NON_EXISTING_PAGE_IN_FRAME;
}

RC unpinPage(BM_BufferPool *const bm, BM_PageHandle *const page) {
   if (!(bm && bm->numPages > 0)) {
    return RC_INVALID_BM;
}

 // Access management information
    BMInfo *info = (BMInfo *)bm->mgmtData;
// Locate the page to be unpinned
    PageFrameNode *f = findNodeByPageNum(info->frames, page->pageNum);
    if (f == NULL) {
        return RC_NON_EXISTING_PAGE_IN_FRAME;
    }
    // Decrease the fix count for the page if it's greater than 0
    if (f->data.fixCount > 0) {
        f->data.fixCount--;
    } else {
        return RC_NON_EXISTING_PAGE_IN_FRAME;
    }

    return RC_OK;
}

RC forcePage(BM_BufferPool *const bm, BM_PageHandle *const page) {
// Check if the buffer pool is valid and contains pages
BMInfo *info = (BMInfo *)bm->mgmtData;
PageFrameNode *f;
SM_FileHandle fh;

if (!bm) {
    return RC_INVALID_BM;
}

if (bm->numPages <= 0) {
    return RC_INVALID_BM;
}

int result = openPageFile(bm->pageFile, &fh);
if (result != RC_OK) {
    return result;
}

    f = findNodeByPageNum(info->frames, page->pageNum);
    if (f == NULL) {
        closePageFile(&fh);
        return RC_NON_EXISTING_PAGE_IN_FRAME;
    }

    if (writeBlock(f->data.pageNum, &fh, f->data.dataPoint) != RC_OK) {
        closePageFile(&fh);
        return RC_WRITE_FAILED;
    }

    info->stats.numWrite++;

    closePageFile(&fh);

    return RC_OK;
}

RC pinPage(BM_BufferPool *const bm, BM_PageHandle *const page,
           const PageNumber pageNum) {
             // Check if the buffer pool is valid and contains pages
   if ((bm == NULL) || (bm->numPages <= 0) || (pageNum < 0)) {
    if (bm == NULL || bm->numPages <= 0) {
        return RC_INVALID_BM;
    } else {
        return RC_READ_NON_EXISTING_PAGE;
    }
}


    if (bm->strategy == RS_FIFO) {
        return pinPage_FIFO(bm, page, pageNum);
    } 
    else if (bm->strategy == RS_LRU) {
        return pinPage_LRU(bm, page, pageNum);
    } 
    else if (bm->strategy == RS_LRU_K) {
        return pinPage_LRU_K(bm, page, pageNum);
    } 
    else {
        return RC_UNKNOWN_STRATEGY;
    }
     // Although unreachable, returning success for consistency
    return RC_OK;
}


/* STATISTICS FUNCTIONS */

PageNumber *getFrameContents(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return NULL;
    }
 // Cast the management data to BMInfo and retrieve the frame-to-page mapping
    BMInfo *info = (BMInfo *)bm->mgmtData;
    return info->frameToPage;
}

bool *getDirtyFlags(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return NULL;// Return NULL if the buffer pool or management data is
                      // invalid
    }

    BMInfo *info = (BMInfo *)bm->mgmtData;
     // Iterate through frames list from the head of the list
    for (PageFrameNode *cur = info->frames->head; cur != NULL;
         cur = cur->nextFrame) {
        info->dirtyFlags[cur->data.frameNum] = cur->data.dirty;
    }
    // Return the updated dirtyFlags array
    return info->dirtyFlags;
}

int *getFixCounts(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return NULL;
    }
    // Cast the management data to BMInfo for easier access
    BMInfo *info = (BMInfo *)bm->mgmtData;
    PageFrameNode *cur = info->frames->head;
    // Use a for loop to iterate through the list of frames
    for (; cur != NULL; cur = cur->nextFrame) {
        info->fixedCounts[cur->data.frameNum] = cur->data.fixCount;
    }

    return info->fixedCounts;
}

int getNumReadIO(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return -1;
    }
    // Cast the management data to BMInfo for easier access and return numRead
    BMInfo *info = (BMInfo *)bm->mgmtData;
    return info->stats.numRead;
}

int getNumWriteIO(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return -1;// Return -1 or an appropriate error code if the buffer pool
                    // is invalid
    }
    return ((BMInfo *)bm->mgmtData)->stats.numWrite;
}

