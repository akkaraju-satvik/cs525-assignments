# CS525 Advanced Database Organization - Assignment 1 - Group 16

> **Group 16 Members**
>
> - Satvik Akkaraju - <sakkaraju@hawk.iit.edu>
> - Manaswini Nagasamudram - <mnagasamudram@hawk.iit.edu>
> - Bindu Bhargavi Palam - <bpalam@hawk.iit.edu>

## Steps to execute the file

- Clone the repository
- Change to assign1_storage_manager directory

  ```bash
  cd assign1_storage_manager
  ```

- Clean the repository of any previous executables and files created from the program using Makefile target `clean`.

  ```bash
  make clean
  ```

- Execute the program using

  ```bash
  make all
  ```

### Function Descriptions

- #### `void initStorageManager (void)`

- #### `RC createPageFile (char *fileName)`

- #### `RC openPageFile (char *fileName, SM_FileHandle *fHandle)`

- #### `RC closePageFile (SM_FileHandle *fHandle)`

- #### `RC destroyPageFile (char *fileName)`

- #### `RC readBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)`

- #### `int getBlockPos (SM_FileHandle *fHandle)`

- #### `RC readFirstBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)`

- #### `RC readPreviousBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)`

- #### `RC readCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)`

- #### `RC readNextBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)`

- #### `RC readLastBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)`

- #### `RC writeBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)`

  - Initial validations are done for pageNum value (it should be a positive integer and if it is less than fHandle->totalNumPages, create new blocks)
  - Open the file containing the pages
  - Seek to the position where the data is to be written
  - Write the memPage data to the file using `fwrite` function.
  - Close the file
  - Return RC as per the success/failure of the function as required.

- #### `RC writeCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)`

  - Open the file if not open yet.
  - Calculate the current page number (currentPageNumber = currentPosition / PAGE_SIZE)
  - Call `writeBlock` function with `currentPageNumber` as the `pageNum` parameter and pass the other arguments accordingly.
  - Increment the number of pages by 1.
  - Return RC as per the success/failure of the function as required.

- #### `RC appendEmptyBlock (SM_FileHandle *fHandle)`

  - Open the file if not open yet.
  - Allocate `PAGE_SIZE*sizeof(char)` memory using `calloc`
  - Write the memory page block to the file using `writeBlock` function.
  - Increment the number of pages by 1.
  - Return RC as per the success/failure of the function as required.

- #### `RC ensureCapacity (int numberOfPages, SM_FileHandle *fHandle)`

  - Open the file if not open yet.
  - Validate the numberOfPages value.
  - Calculate how many blocks are to be added.
  - Use `appendNewBlock` function to add new pages at the end of the file.
  - Increment the number of pages by number of new pages added to the file.
  - Check if the required number of pages are added to the file.
  - Return RC as per the success/failure of the function as required.

### Utility functions

- #### `void makePointerNull()`

- #### `FileDetails openFile(char *fileName, char *mode)`

  - Initialize a FileDetails variable with returnCode to be RC_OK.
  - Open the file with required `mode`.
  - Set the fileDetails.file to the opened file pointer.
  - If the file is not opened as intended, set the returnCode of fileDetails to be an appropriate returnCode from `dberror.h`.
