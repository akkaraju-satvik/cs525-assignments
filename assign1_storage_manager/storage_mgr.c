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

RC createPageFile(char *fileName) {
  char *record = fileName;
  fileHandlerPointer = fopen(record, "w+");
  if (fileHandlerPointer == nullValueCheck) {
    return RC_FILE_NOT_FOUND;
  } else {
    SM_PageHandle emptyPageHandler =
        (SM_PageHandle)calloc(PAGE_SIZE, sizeof(char));
    int writeFileStatus =
        fwrite(emptyPageHandler, sizeof(char), PAGE_SIZE, fileHandlerPointer);
    if (writeFileStatus == 0) {
      return RC_WRITE_FAILED;
    }
    return RC_OK;
  }
}

RC openPageFile(char *fileName, SM_FileHandle *fHandle) {
  FILE *fn;
  fn = fopen(fileName, "r");
  if (fn == NULL) {
    return RC_FILE_NOT_FOUND;
  }
  fHandle->fileName = fileName;
  fHandle->mgmtInfo = file;
  fseek(file, 0, SEEK_END);     // shifting to the end of the file
  long fileSize = ftell(file);  // to get the file size
  fHandle->totalNumPages = fileSize / PAGE_SIZE;
  fseek(file, 0, SEEK_SET);
  fHandle->curPagePos = 0;
  return RC_OK;
}

RC closePageFile(SM_FileHandle *fHandle) {
  FILE *fn = fopen(fHandle->fileName, "rb+");
  if (fn != NULL) {
    int position = fseek(fn, 0, SEEK_SET);
    if (position == 0) {
      fclose(fn);
      fn = NULL;
      return RC_OK;
    }
  }
  return RC_FILE_NOT_FOUND;
}

RC destroyPageFile(char *fileName) {
  FILE *fn = fopen(fileName, "rb+");
  if (fn != NULL) {
    fclose(fn);
    int mode = remove(fileName);
    if (mode == 0) {
      return RC_OK;
    }
  }

  return RC_FILE_NOT_FOUND;
}

RC readBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {
  FILE *fn = fopen(fHandle->fileName, "r");
  if (pageNum <= 0 || pageNum > fHandle->totalNumPages) {
    return RC_READ_NON_EXISTING_PAGE;
  }
  if (fn != NULL) {
    int temp = fseek(fn, (pageNum * PAGE_SIZE), SEEK_SET);
    if (temp == 0) {
      fread(memPage, PAGE_SIZE, sizeof(char), fn);
      fHandle->curPagePos = ftell(fn);
      fclose(fn);
      return RC_OK;
    } else {
      return RC_READ_NON_EXISTING_PAGE;
    }
  }
  fclose(fn);
  return RC_FILE_NOT_FOUND;
}

int getBlockPos(SM_FileHandle *fHandle) {
  FILE *fn = fopen(fHandle->fileName, "r");
  if (fn != NULL) {
    int currentPage = fHandle->curPagePos;
    return currentPage;
  }
  return 0;
}

RC readFirstBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
  return readBlock(0, fHandle, memPage);
}

RC readLastBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
  return readBlock(fHandle->totalNumPages - 1, fHandle, memPage);
}

RC readPreviousBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
  int readPreviousBlock = getBlockPos(fHandle) - 1;
  return readBlock(readPreviousBlock, fHandle, memPage);
}

RC readCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
  int readCurrentBlock = fHandle->curPagePos / PAGE_SIZE;
  return readBlock(readCurrentBlock, fHandle, memPage);
}

RC readNextBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
  int readNextBlock = getBlockPos(fHandle) + 1;
  return readBlock(readNextBlock, fHandle, memPage);
}
