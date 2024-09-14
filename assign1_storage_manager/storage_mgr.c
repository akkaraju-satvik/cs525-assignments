#include "storage_mgr.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  FILE *file;
  RC returnCode;
} FileDetails;

FILE *fileHandlerPointer;
FILE *nullValueCheck = NULL;

// Method to make the pointer values as null.
void makePointerNull() { fileHandlerPointer = nullValueCheck; }

void initStorageManager(void) {
  do {
    makePointerNull();  // Initialize the file pointer to NULL value
  } while (0);
}

int seekFilePointer(FILE *fileHandlerPointer, long int n, int position) {
  return fseek(fileHandlerPointer, n, position);
}

void fcloseFilePointer(FILE *fileHandlerPointer) { fclose(fileHandlerPointer); }

FileDetails openFile(char *fileName, char *mode) {
  FileDetails fileError;
  fileError.returnCode = RC_OK;
  FILE *file = fopen(fileName, mode);
  fileError.file = file;
  if (file == NULL) fileError.returnCode = RC_FILE_NOT_FOUND;
  return fileError;
}

/*
  @brief
    Creates a new page file.
  @details
    This function creates a new file with a single empty page of size PAGE_SIZE (4096). 
    If the file cannot be created, it returns an error code.
  @category file operations

  @param fileName - The name of the file to be created.

  @return RC_OK if the file is created successfully.
  @return RC_FILE_NOT_FOUND if the file could not be created.
  @return RC_WRITE_FAILED if writing the empty page fails.
*/

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
    fcloseFilePointer(fileHandlerPointer);
    return RC_OK;
  }
}

/*
  @brief
    Opens an existing file
  @details
    this function opens an existing file and sets the file handle
  @category file operations
  @param fileName - The name of the file to be created.
  @return RC_FILE_NOT_FOUND if the file could not be created.
  @return RC_OK if the file is created successfully.
*/

RC openPageFile(char *fileName, SM_FileHandle *fHandle) {
  char *record = fileName;
  fileHandlerPointer = fopen(record, "r");
  if (fileHandlerPointer == NULL) {
    return RC_FILE_NOT_FOUND;
  }
  fHandle->fileName = record;
  fHandle->curPagePos = 0;
  seekFilePointer(fileHandlerPointer, 0, SEEK_END);
  long fileSize = ftell(fileHandlerPointer);  // to get the file size
  fHandle->totalNumPages = fileSize / PAGE_SIZE;
  fcloseFilePointer(fileHandlerPointer);
  return RC_OK;
}
/*
  @brief
    close an existing file
  @details
    this function closes the file given in the fhandle. 
    the pointer will be set to the starting position of the file before closing it.
  @category file operations
  @param fHandle - The file handle that stores the metadata of the file.
  @return RC_FILE_NOT_FOUND if the file could not be created.
  @return RC_OK if the file is created successfully.
*/

RC closePageFile(SM_FileHandle *fHandle) {
  FILE *fileHandlerPointer = fopen(fHandle->fileName, "rb+");
  if (fileHandlerPointer != NULL) {
    int position = seekFilePointer(fileHandlerPointer, 0, SEEK_SET);
    if (position == 0) {
      fcloseFilePointer(fileHandlerPointer);
      fileHandlerPointer = NULL;
      return RC_OK;
    }
  }
  return RC_FILE_NOT_FOUND;
}

/*
  @brief
    removes the created file
  @details
    Deletes the file from the system after closing it only if it exists.
  @category file operations
  @param fHandle - The file handle that stores the metadata of the file.
  @return RC_FILE_NOT_FOUND if the file could not be created.
  @return RC_OK if the file is created successfully.
*/
RC destroyPageFile(char *fileName) {
  FILE *fileHandlerPointer = fopen(fileName, "rb+");
  if (fileHandlerPointer != NULL) {
    fcloseFilePointer(fileHandlerPointer);
    int mode = remove(fileName);
    if (mode == 0) {
      return RC_OK;
    }
  }

  return RC_FILE_NOT_FOUND;
}

/*
  @brief
    reads a block from the file at the specified page number in the file
  @details
    This function reads a block of data from a specific page number into the memory page handler.
  @category read operations

  @param pageNum - The page number to read from.
  @param fHandle - The file handle that stores the metadata of the file.
  @param memPage - The memory page to store the read data.

  @return RC_FILE_NOT_FOUND if the file could not be created.
  @return RC_OK if the file is created successfully.
  @return RC_READ_NON_EXISTING_PAGE if the page number is invalid.
*/

RC readBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {
  FILE *fileHandlerPointer = fopen(fHandle->fileName, "r");
  if (pageNum < 0 || pageNum > fHandle->totalNumPages) {
    return RC_READ_NON_EXISTING_PAGE;
  }
  if (fileHandlerPointer != NULL) {
    int temp =
        seekFilePointer(fileHandlerPointer, (pageNum * PAGE_SIZE), SEEK_SET);
    if (temp == 0) {
      fread(memPage, PAGE_SIZE, sizeof(char), fileHandlerPointer);
      fHandle->curPagePos = ftell(fileHandlerPointer);
      fcloseFilePointer(fileHandlerPointer);
      return RC_OK;
    } else {
      return RC_READ_NON_EXISTING_PAGE;
    }
  }
  fcloseFilePointer(fileHandlerPointer);
  return RC_FILE_NOT_FOUND;
}

/*
  @brief
    Gets the current block position.
  @details
    Returns the current position of the file pointer based on the page number.
  @category read operations

  @param fHandle - The file handle that stores the metadata of the file.

  @return int - The current page position.
*/
int getBlockPos(SM_FileHandle *fHandle) {
  FILE *fileHandlerPointer = fopen(fHandle->fileName, "r");
  if (fileHandlerPointer != NULL) {
    int currentPage = fHandle->curPagePos;
    return currentPage;
  }
  return 0;
}

/*
  @brief
    Reads the first block from the file.
  @details
    Reads the first page of the file into the memory page handler.
  @category read page operations

  @param fHandle - The file handle that stores the metadata of the file.
  @param memPage - The memory page to store the read data.
*/

RC readFirstBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
  return readBlock(0, fHandle, memPage);
}


/*
  @brief
    Reads the last block from the file.
  @details
    Reads the last page of the file into the memory page handler.
  @category read page operations

  @param fHandle - The file handle that stores the metadata of the file.
  @param memPage - The memory page to store the read data.
*/


RC readLastBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
  return readBlock(fHandle->totalNumPages - 1, fHandle, memPage);
}

/*
  @brief
    Reads the previous block based on the current position.
  @details
    Reads the page immediately before the current page into the memory page handler.
  @category read page operations

  @param fHandle - The file handle that stores the metadata of the file.
  @param memPage - The memory page to store the read data.
*/

RC readPreviousBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
  int readPreviousBlock = getBlockPos(fHandle) - 1;
  return readBlock(readPreviousBlock, fHandle, memPage);
}

/*
  @brief
    Reads the current block.
  @details
    This function reads the current page of the file into the memory page handler.
  @category read page operations

  @param fHandle - The file handle that stores the metadata of the file.
  @param memPage - The memory page to store the read data.
*/

RC readCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
  int readCurrentBlock = fHandle->curPagePos / PAGE_SIZE;
  return readBlock(readCurrentBlock, fHandle, memPage);
}


/*
  @brief
    Reads the next block relative to the current position.
  @details
    Reads the page immediately after the current page into the memory page handler.
  @category read page operations

  @param fHandle - The file handle that stores the metadata of the file.
  @param memPage - The memory page to store the read data.
*/

RC readNextBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
  int readNextBlock = getBlockPos(fHandle) + 1;
  return readBlock(readNextBlock, fHandle, memPage);
}

/*
  @brief
    Write a new block at an absolute position of the file
  @details

  @category write/append page operations

  @param pageNum - the page number where the write operation is required
  @param fHandle - SM_FileHandle that stores the metadata of the program
  @param memPage - The data to be written to the page

  @return RC_OK if the write is success
  @return error from openFile fn if the errors are related to the file
  @return RC_WRITE_FAILED if the write fails

  @author Satvik Akkaraju
  @date 09/12/2024
*/
RC writeBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {
  RC returnCode = RC_OK;
  // pageNum validations
  if (pageNum < 0) {
    returnCode = RC_WRITE_FAILED;
    return returnCode;
  }
  if (pageNum > fHandle->totalNumPages) {
    ensureCapacity(pageNum, fHandle);
  }
  FileDetails fileDetails = openFile(fHandle->fileName, "r+");
  if (fileDetails.returnCode != RC_OK) {
    fcloseFilePointer(fileHandlerPointer);
    return fileDetails.returnCode;
  }
  fileHandlerPointer = fileDetails.file;
  if (fseek(fileHandlerPointer, (pageNum * PAGE_SIZE), SEEK_SET) != 0) {
    returnCode = RC_READ_NON_EXISTING_PAGE;
    return returnCode;
  }
  int writeSize = fwrite(memPage, sizeof(char), PAGE_SIZE, fileHandlerPointer);
  if (writeSize != PAGE_SIZE * sizeof(char)) {
    returnCode = RC_WRITE_FAILED;
    return returnCode;
  }
  fcloseFilePointer(fileHandlerPointer);
  return returnCode;
}

/*
  @brief
    Write a new block at the current position of the file
  @details
    Calculates the current page number and calls writeBlock with the position to
    write "memPage" data at the current position of the file.
  @category write/append page operations

  @param fHandle - SM_FileHandle that stores the metadata of the program
  @param memPage - The data that is to be written to the page

  @return RC_OK if the write is success
  @return error from openFile fn if the errors are related to the file
  @return RC_WRITE_FAILED if the write fails

  @author Satvik Akkaraju
  @date 09/12/2024
*/
RC writeCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
  RC returnCode = RC_OK;
  if (fileHandlerPointer == NULL) {
    FileDetails fileDetails = openFile(fHandle->fileName, "r+");
    if (fileDetails.returnCode != RC_OK) {
      fcloseFilePointer(fileHandlerPointer);
      return fileDetails.returnCode;
    }
    fileHandlerPointer = fileDetails.file;
  }
  // calculating the currentPageNumber (pageNumber = currentFilePosition /
  // PAGE_SIZE)
  int currentPageNumber = fHandle->curPagePos / PAGE_SIZE;
  RC writeReturnCode = writeBlock(currentPageNumber + 1, fHandle, memPage);
  if (writeReturnCode != RC_OK) {
    returnCode = writeReturnCode;
    fcloseFilePointer(fileHandlerPointer);
    return returnCode;
  }
  ++fHandle->totalNumPages;
  fcloseFilePointer(fileHandlerPointer);
  return returnCode;
}

/*
  @brief
    Append a new page at the end of the file
  @details
    Appends an empty block of size PAGE_SIZE*sizeof(char) to the end of the file
  @category write/append page operations

  @param fHandle - SM_FileHandle that stores the metadata of the program

  @return RC_OK if the append is success
  @returns error from openFile fn if the errors are related to the file
  @return RC_WRITE_FAILED if the append fails

  @author Satvik Akkaraju
  @date 09/11/2024
*/
RC appendEmptyBlock(SM_FileHandle *fHandle) {
  RC returnCode = RC_OK;
  if (fileHandlerPointer == NULL) {
    FileDetails fileDetails = openFile(fHandle->fileName, "a+");
    if (fileDetails.returnCode != RC_OK) {
      fcloseFilePointer(fileHandlerPointer);
      return fileDetails.returnCode;
    }
    fileHandlerPointer = fileDetails.file;
  }
  SM_PageHandle pageInMemory = (SM_PageHandle)calloc(PAGE_SIZE, sizeof(char));
  RC writeReturnCode =
      writeBlock(fHandle->totalNumPages + 1, fHandle, pageInMemory);
  if (writeReturnCode != RC_OK) {
    fcloseFilePointer(fileHandlerPointer);
    return writeReturnCode;
  }
  ++fHandle->totalNumPages;
  fcloseFilePointer(fileHandlerPointer);
  return returnCode;
}

/*
  @brief
    Ensure that the file has atleast "numberOfPages" pages in it.
  @details
    If the file does not have "numberOfPages" pages in it, we append new
    pages to it using appendEmptyBlock function.
  @category - write/append page operations

  @param numberOfPages - Total number of pages to be present in the file.
  @param fHandle - SM_FileHandle that stores the metadata of the program

  @return RC_OK if appends are success.
  @return RC_READ_NON_EXISTING_PAGE if numberOfPages < fHandle->totalNumPages
  @return RC_WRITE_FAILED if any appendEmptyBlock call returns error.

  @author Satvik Akkaraju
  @date 09/11/2024
*/
RC ensureCapacity(int numberOfPages, SM_FileHandle *fHandle) {
  RC returnCode = RC_OK;
  if (fileHandlerPointer == NULL) {
    FileDetails fileOrError = openFile(fHandle->fileName, "a+");
    if (fileOrError.returnCode != RC_OK) {
      fcloseFilePointer(fileHandlerPointer);
      return fileOrError.returnCode;
    }
    fileHandlerPointer = fileOrError.file;
  }
  if (numberOfPages < fHandle->totalNumPages) {
    returnCode = RC_READ_NON_EXISTING_PAGE;
    fcloseFilePointer(fileHandlerPointer);
    return returnCode;
  }
  int numberOfNewBlocksToAdd = numberOfPages - fHandle->totalNumPages;
  int i = 0;
  while (i < numberOfNewBlocksToAdd) {
    RC appendReturnCode = appendEmptyBlock(fHandle);
    if (appendReturnCode != RC_OK) {
      returnCode = RC_WRITE_FAILED;
      fcloseFilePointer(fileHandlerPointer);
      return returnCode;
    }
    ++fHandle->totalNumPages;
    ++i;
  }
  if (i != numberOfNewBlocksToAdd) {
    returnCode = RC_WRITE_FAILED;
    fcloseFilePointer(fileHandlerPointer);
    return returnCode;
  }
  fcloseFilePointer(fileHandlerPointer);
  return returnCode;
}