#include "storage_mgr.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *file;

FILE *fileHandlerPointer;
FILE *nullValueCheck = NULL;

// Method to make the pointer values as null.
void makePointerNull() { fileHandlerPointer = nullValueCheck; }

void initStorageManager(void) {
  do {
    makePointerNull();  // Initialize the file pointer to NULL value
  } while (0);
}

RC createPageFile(char *fileName) {}

RC openPageFile(char *fileName, SM_FileHandle *fHandle) {}

RC closePageFile(SM_FileHandle *fHandle) {}

RC destroyPageFile(char *fileName) {}

RC readBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {}

int getBlockPos(SM_FileHandle *fHandle) {}

RC readFirstBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {}

RC readLastBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {}

RC readPreviousBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {}

RC readCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {}

RC readNextBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {}

RC writeBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {}

RC writeCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {}

RC appendEmptyBlock(SM_FileHandle *fHandle) {}

RC ensureCapacity(int numberOfPages, SM_FileHandle *fHandle) {}
