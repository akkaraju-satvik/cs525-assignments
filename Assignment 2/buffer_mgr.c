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
    if (updateNode == NULL || (*list)->head == NULL ||
        (*list)->head == updateNode) {
        return;
    }

    PageFrameNode *currentHead = (*list)->head;

    if (updateNode == (*list)->tail) {
        (*list)->tail = updateNode->previousFrame;
        (*list)->tail->nextFrame = NULL;  // New tail's nextFrame is NULL
    } else {
        updateNode->previousFrame->nextFrame = updateNode->nextFrame;
        updateNode->nextFrame->previousFrame = updateNode->previousFrame;
    }
    updateNode->nextFrame = currentHead;
    currentHead->previousFrame = updateNode;
    updateNode->previousFrame = NULL;

    (*list)->head = updateNode;
}

PageFrameNode *findNodeByPageNum(FrameList *list, const PageNumber pageNum) {
    for (PageFrameNode *current = list->head; current != NULL;
         current = current->nextFrame) {
        if (current->data.pageNum == pageNum) {
            return current;
        }
    }

    return NULL;
}

PageFrameNode *pageInMemory(BM_BufferPool *const bm, BM_PageHandle *const page,
                            const PageNumber pageNum) {
    BMInfo *info = (BMInfo *)bm->mgmtData;

    if (info->pageToFrame[pageNum] == NO_PAGE) {
        return NULL;
    }

    PageFrameNode *found = findNodeByPageNum(info->frames, pageNum);
    if (found == NULL) {
        return NULL;
    }

    page->pageNum = pageNum;
    page->data = found->data.dataPoint;

    found->data.fixCount++;
    found->data.referenceBit = 1;

    return found;
}

RC updateNewFrame(BM_BufferPool *const bm, PageFrameNode *found,
                  BM_PageHandle *const page, const PageNumber pageNum) {
    SM_FileHandle fh;
    BMInfo *info = (BMInfo *)bm->mgmtData;
    RC status;

    if (status = openPageFile((char *)(bm->pageFile), &fh), status != RC_OK) {
        return status;
    }

    if (found == NULL) {
        return RC_NON_EXISTING_PAGE_IN_FRAME;
    }

    if (found->data.dirty) {
        if (status = ensureCapacity(pageNum, &fh), status != RC_OK) {
            return status;
        }
        if (status =
                writeBlock(found->data.pageNum, &fh, found->data.dataPoint),
            status != RC_OK) {
            return status;
        }
        info->stats.numWrite++;
    }

    info->pageToFrame[found->data.pageNum] = NO_PAGE;

    status = ensureCapacity(pageNum, &fh);
    if (status != RC_OK) {
        return status;
    }

    status = readBlock(pageNum, &fh, found->data.dataPoint);
    if (status != RC_OK) {
        return status;
    }

    page->pageNum = pageNum;
    page->data = found->data.dataPoint;

    info->stats.numRead++;

    found->data.dirty = 0;
    found->data.fixCount = 1;
    found->data.pageNum = pageNum;
    found->data.referenceBit = 1;

    info->pageToFrame[found->data.pageNum] = found->data.frameNum;
    info->frameToPage[found->data.frameNum] = found->data.pageNum;
    closePageFile(&fh);

    return RC_OK;
}

/* PAGE REPLACEMENT STRATEGIES */

RC pinPage_FIFO(BM_BufferPool *const bm, BM_PageHandle *const page,
                const PageNumber pageNum) {
    // Initialize variables
    PageFrameNode *found;
    BMInfo *info = (BMInfo *)bm->mgmtData;

    if ((found = pageInMemory(bm, page, pageNum)), found != NULL) {
        return RC_OK;  // Page is already in memory, return success
    }

    if (info->numOfFrames < bm->numPages) {
        found = info->frames->head;
        for (int i = 0; i < info->numOfFrames; ++i) {
            found = found->nextFrame;
        }

        info->numOfFrames++;
        updateCursor(&(info->frames),
                     found);
    } else {
        found = info->frames->tail;
        while (found != NULL && found->data.fixCount != 0) {
            found = found->previousFrame;
        }

        if (found == NULL) {
            return RC_NO_MORE_SPACE_IN_BUFFER;  // Buffer is full
        }

        updateCursor(
            &(info->frames),
            found);
    }

    RC status = updateNewFrame(bm, found, page, pageNum);
    if (status != RC_OK) {
        return status;
    }

    return RC_OK;
}

RC pinPage_LRU(BM_BufferPool *const bm, BM_PageHandle *const page,
               const PageNumber pageNum) {
    PageFrameNode *found;
    BMInfo *info = (BMInfo *)bm->mgmtData;

    RC status;

    if ((found = pageInMemory(bm, page, pageNum)), found != NULL) {
        updateCursor(&(info->frames), found);
        status = RC_OK;
        return status;
    }

    if (info->numOfFrames < bm->numPages) {
        found = info->frames->head;
        for (int i = 0; i < info->numOfFrames; ++i) {
            found = found->nextFrame;
        }
        ++info->numOfFrames;
    } else {
        found = info->frames->tail;
        while (found != NULL && found->data.fixCount != 0) {
            found = found->previousFrame;
        }
    }

    if (status = updateNewFrame(bm, found, page, pageNum), status != RC_OK) {
        return status;
    }

    updateCursor(&(info->frames), found);

    return RC_OK;
}

RC pinPage_LRU_K(BM_BufferPool *const bm, BM_PageHandle *const page,
                 const PageNumber pageNum) {
    PageFrameNode *found;
    BMInfo *info = (BMInfo *)bm->mgmtData;
    int K = (long)(info->stratData);
    int i;
    (info->stats.numPins)++;

    if ((found = pageInMemory(bm, page, pageNum)) != NULL) {
        for (i = K - 1; i > 0; i--) {
            info->khist[found->data.pageNum][i] =
                info->khist[found->data.pageNum][i - 1];
        }

        info->khist[found->data.pageNum][0] = info->stats.numPins;

        return RC_OK;
    }

    if ((info->numOfFrames) < bm->numPages) {
        found = info->frames->head;

        int i = 0;
        while (i < info->numOfFrames) {
            found = found->nextFrame;
            ++i;
        }

        (info->numOfFrames)++;
    } else {
        PageFrameNode *current;
        int dist, max_dist = -1;

        current = info->frames->head;

        while (current != NULL) {
            if (current->data.fixCount == 0 &&
                info->khist[current->data.pageNum][K] != -1) {
                dist =
                    info->stats.numPins - info->khist[current->data.pageNum][K];

                if (dist > max_dist) {
                    max_dist = dist;
                    found = current;
                }
            }
            current = current->nextFrame;
        }

        if (max_dist == -1) {
            current = info->frames->head;
            while (current->data.fixCount != 0 && current != NULL) {
                dist =
                    info->stats.numPins - info->khist[current->data.pageNum][0];
                if (dist > max_dist) {
                    max_dist = dist;
                    found = current;
                }
                current = current->nextFrame;
            }

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
        info->khist[found->data.pageNum][i] =
            info->khist[found->data.pageNum][i - 1];
    }
    info->khist[found->data.pageNum][0] = info->stats.numPins;

    return RC_OK;
}

/*Buffer Pool Functions*/

RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,
                  const int numPages, ReplacementStrategy strategy,
                  void *stratData) {
    if (numPages <= 0) {
        return RC_INVALID_BM;
    }

    SM_FileHandle fh;
    if (openPageFile((char *)pageFileName, &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;  // File not found
    }

    BMInfo *info = malloc(sizeof(BMInfo));

    info->stats.numRead = 0;
    info->stats.numWrite = 0;
    info->stratData = stratData;
    info->stats.numPins = 0;

    memset(info->frameToPage, NO_PAGE, MAX_FRAMES * sizeof(int));
    memset(info->pageToFrame, NO_PAGE, MAX_PAGES * sizeof(int));
    info->numOfFrames = 0;
    memset(info->dirtyFlags, NO_PAGE, MAX_FRAMES * sizeof(bool));
    memset(info->fixedCounts, NO_PAGE, MAX_FRAMES * sizeof(int));
    memset(info->khist, -1, sizeof(info->khist));  // Correct usage of sizeof
    memset(info->pageToFrequency, 0, MAX_PAGES * sizeof(int));

    info->frames = malloc(sizeof(FrameList));

    info->frames->head = info->frames->tail = newNode();

    int i = 1;
    while (i < numPages) {
        PageFrameNode *newFrame = newNode();
        info->frames->tail->nextFrame = newFrame;
        newFrame->previousFrame = info->frames->tail;
        info->frames->tail = newFrame;
        info->frames->tail->data.frameNum = i;
        i++;
    }

    bm->numPages = numPages;
    bm->pageFile = (char *)pageFileName;
    bm->strategy = strategy;
    bm->mgmtData = info;

    closePageFile(&fh);

    return RC_OK;
}

RC shutdownBufferPool(BM_BufferPool *const bm) {
    if (!bm || bm->numPages <= 0) {
        return RC_INVALID_BM;
    }

    RC status;

    if ((status = forceFlushPool(bm)), status != RC_OK) {
        return status;
    }

    BMInfo *info = (BMInfo *)bm->mgmtData;
    PageFrameNode *current = info->frames->head;

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
    if (!bm || bm->numPages <= 0) {
        return RC_INVALID_BM;
    }

    BMInfo *info = (BMInfo *)bm->mgmtData;
    PageFrameNode *current = info->frames->head;

    SM_FileHandle fh;
    if (openPageFile((char *)(bm->pageFile), &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;
    }

    for (; current != NULL; current = current->nextFrame) {
        if (current->data.dirty) {
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
    if (!bm || bm->numPages <= 0) {
        return RC_INVALID_BM;
    }
    BMInfo *info = (BMInfo *)bm->mgmtData;
    PageFrameNode *found = findNodeByPageNum(info->frames, page->pageNum);
    if (found != NULL) {
        found->data.dirty = true;

        return RC_OK;
    }

    return RC_NON_EXISTING_PAGE_IN_FRAME;
}

RC unpinPage(BM_BufferPool *const bm, BM_PageHandle *const page) {
    if (!bm || bm->numPages <= 0) {
        return RC_INVALID_BM;
    }

    BMInfo *info = (BMInfo *)bm->mgmtData;

    PageFrameNode *found = findNodeByPageNum(info->frames, page->pageNum);
    if (found == NULL) {
        return RC_NON_EXISTING_PAGE_IN_FRAME;
    }
    if (found->data.fixCount > 0) {
        found->data.fixCount--;
    } else {
        return RC_NON_EXISTING_PAGE_IN_FRAME;
    }

    return RC_OK;
}

RC forcePage(BM_BufferPool *const bm, BM_PageHandle *const page) {
    if (!bm || bm->numPages <= 0) {
        return RC_INVALID_BM;
    }

    BMInfo *info = (BMInfo *)bm->mgmtData;
    PageFrameNode *found;
    SM_FileHandle fh;

    if (openPageFile((char *)(bm->pageFile), &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;
    }

    found = findNodeByPageNum(info->frames, page->pageNum);
    if (found == NULL) {
        closePageFile(&fh);
        return RC_NON_EXISTING_PAGE_IN_FRAME;
    }

    if (writeBlock(found->data.pageNum, &fh, found->data.dataPoint) != RC_OK) {
        closePageFile(&fh);
        return RC_WRITE_FAILED;
    }

    info->stats.numWrite++;

    closePageFile(&fh);

    return RC_OK;
}

RC pinPage(BM_BufferPool *const bm, BM_PageHandle *const page,
           const PageNumber pageNum) {
    if (!bm || bm->numPages <= 0) {
        return RC_INVALID_BM;
    }

    if (pageNum < 0) {
        return RC_READ_NON_EXISTING_PAGE;
    }

    switch (bm->strategy) {
        case RS_FIFO:
            return pinPage_FIFO(bm, page, pageNum);
        case RS_LRU:
            return pinPage_LRU(bm, page, pageNum);
        case RS_LRU_K:
            return pinPage_LRU_K(bm, page, pageNum);
        default:
            return RC_UNKNOWN_STRATEGY;
    }
    return RC_OK;
}

/* STATISTICS FUNCTIONS */

PageNumber *getFrameContents(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return NULL;
    }

    BMInfo *info = (BMInfo *)bm->mgmtData;
    return info->frameToPage;
}

bool *getDirtyFlags(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return NULL;
    }

    BMInfo *info = (BMInfo *)bm->mgmtData;

    for (PageFrameNode *cur = info->frames->head; cur != NULL;
         cur = cur->nextFrame) {
        info->dirtyFlags[cur->data.frameNum] = cur->data.dirty;
    }

    return info->dirtyFlags;
}

int *getFixCounts(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return NULL;
    }

    BMInfo *info = (BMInfo *)bm->mgmtData;
    PageFrameNode *cur = info->frames->head;

    for (; cur != NULL; cur = cur->nextFrame) {
        info->fixedCounts[cur->data.frameNum] = cur->data.fixCount;
    }

    return info->fixedCounts;
}

int getNumReadIO(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return -1;
    }
    BMInfo *info = (BMInfo *)bm->mgmtData;
    return info->stats.numRead;
}

int getNumWriteIO(BM_BufferPool *const bm) {
    if (bm == NULL || bm->mgmtData == NULL) {
        return -1;
    }
    return ((BMInfo *)bm->mgmtData)->stats.numWrite;
}
