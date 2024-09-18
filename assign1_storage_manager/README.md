# CS525 Advanced Database Organization - Assignment 1 - Group 16

> **Group 16 Members**
>
> - Satvik Akkaraju - <sakkaraju@hawk.iit.edu>
> - Manaswini Nagasamudram - <mnagasamudram@hawk.iit.edu>
> - Bindu Bhargavi Palam - <bpalam@hawk.iit.edu>

## Contributions

### Manaswini Nagasamudram

- Implemented the functions for read operations, including readBlock, getBlockPos, readFirstBlock, readLastBlock, readCurrentBlock, readPreviousBlock and readNextBlock.
- During the implementation, it was observed that some functions(readFirstBlock(), readLastBlock(), readCurrentBlock(), readPreviousBlock() and readNextBlock()) shared similar functionality. To avoid redundancy, allowing these functions to reference each other and consistent use of readBlock() and getBlockPos() functions.
- Created a Makefile to automate the build process for compling and linking the code files.
- Additionally, comprehensive documentation was added to the implemented functions, detailing the operations performed to meet the requirements outlined in the assignment instructions.
- Implemented Error handling to manage cases such as reading non0esisting pages, missing files,file related issues and non-existing blocks were carefully managed to ensure that any issues from called functions were properly addressed. Clear documentation was provided for each function.


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

  - This function initializes the storage manager.
  - It assigns the global object created for the file stream to NULL using a function within this method.

- #### `RC createPageFile (char *fileName)`

  - This function used for creating a new page file and takes file name as in the argument passed to it.
  - It open the file using the fopen function in write mode ('w+'). If file cannot be opened, the function returns the RC_FILE_NOT_FOUND.
  - If the file is successfully opened, an empty page of size PAGE_SIZE is initialized using calloc.
  - fwrite function is used to write empty page to new file created. If writing fails, then function returns RC_WRITE_FAILED.
  - The file is closed using fcloserFilePointer function. If all operations are successful, the function return RC_OK.

- #### `RC openPageFile (char *fileName, SM_FileHandle *fHandle)`

  - This function is for opening an exisiting page file and initialize SM_FileHandle for file management.
  - It takes the file name in the argument passed to it that needs to be opened. and fHandle pointer to initialize with files metadata.
  - This function open the file in read mode ("r") and If the file is not found, then function returns RC_FILE_NOT_FOUND.
  - Once the file is succesfully opened, fileName is assigned in fHandle and the curPagePos ( current page position) is set to 0.
  - Seek the file pointer and close after reading the files information using fcloseFilePointer.
  - Return RC as per the success/failure of the function as required.

- #### `RC closePageFile (SM_FileHandle *fHandle)`

  - This function is used to close an open page file in storage manager.
  - Return RC_OK, if file is closed.

- #### `RC destroyPageFile (char *fileName)`

  - This function destroy the sile created.
  - Open the file and check if not NULL
  - Remove the file pointer, if file exists and return RC_OK.
  - Return RC_FILE_NOT_FOUND, if file doesn't exist.

- #### `RC readBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)`

  - This function reads a block from disk into memory.
  - File is opened only for reading using fopen in 'r'mode.
  - Navigating file pointer using fseek() based on page number and page size.
  - Read data and store at starting of the page handle.
  - Verify the page number is not negative and larger than the total pages.

- #### `int getBlockPos (SM_FileHandle *fHandle)`

  - This function returns the current page position by aaccessing the currrent page number in the file Handle.
  - Managed file opening and ensuring that if the file pointer is NULL, it returns 0 as a default value.

- #### `RC readFirstBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)

  - This function is to read the first block(page 0)from the file.
  - Used readblock() with Page number as o to read first files first page.

- #### `RC readPreviousBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)`

  - This function is to calculate the previous page number using the file current page position.
  - Calling readblock(), where the current page -1 is assigned as previous page number to read the page.

- #### `RC readCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)`

  - This function is to read the current page using file's current page position.
  - Assigning current page number to current page position and calculating current page number.
  - Calling readblock() methog with current page position by the page size.

- #### `RC readNextBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)`

  - This function is to read the next block after the current block by incrementing the current page position.
  - Used getBlockPos() to retrieve the current position and called readblock() with current page position+1 as the page number to handle the read operation for the next block.

- #### `RC readLastBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)`

  - This function is to read the last block in the file by using the total number of pages and subtracting 1 to get the last block index.
  - Used readblock() with last block index as the page number to handle the read operation for the last block.

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

- #### `FileDetails openFile(char *fileName, char *mode)`

  - Initialize a FileDetails variable with returnCode to be RC_OK.
  - Open the file with required `mode`.
  - Set the fileDetails.file to the opened file pointer.
  - If the file is not opened as intended, set the returnCode of fileDetails to be an appropriate returnCode from `dberror.h`.
