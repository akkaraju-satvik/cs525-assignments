# CS525 Advanced Database Organization - Assignment 2 - Group 16

> **Group 16 Members**
>
> - Satvik Akkaraju - <sakkaraju@hawk.iit.edu>
> - Manaswini Nagasamudram - <mnagasamudram@hawk.iit.edu>
> - Bindu Bhargavi Palam - <bpalam@hawk.iit.edu>

## Contributions

### Manaswini Nagasamudram
- Designed the page management functions that contain the pinPage and unpinPage with following strategies: FIFO, LRU, CLOCK, LFU, and LRU-K.
- Implemented utility functions updateHead(), to make the given node the head of the list; findNodeByPageNum(), to find the node with the given page number - first check by the lookup array, and if available in memory, then to find the exact node; findNodeByPageNum(). UpdateNewFrame().
- Implemented page replacement strategies of FIFO, LRU, LFU, CLOCK, and LRU-K with pinPage functions in a buffer management system. Each of these functions takes up memory management, page loading, and page replacement according to the respective strategy to perform optimally within the usage of resources of the buffer pool. It deals with managing lists of frames, updating metadata of pages, and ensuring the correct application of replacement algorithms.

### Satvik Akkaraju

### Bindu Bhargavi Palam
- Implemented "makeDirty". In this function it marks a specific page as "dirty" in the buffer pool in the disk, showcasing that the page has been modified. and it also ensures whether it is being written in the disk or not. it make sures that the buffer pool is valid and checks the page is present before marking it dirty. 
- Implemented "getFrameContents" method returning current page mapping stored in the buffer pool and returning an array of page numbers corresponding to every frame.
- Implemented "getDirtyFlags", which updates and returns the dirty flags in the buffer pool of all the frames in the disk, ensuring that each frame has been modified.
- Implemented "getFixCounts" method to update and then return the fix counts in the buffer pool of every frame. It now returns how many users are currently using each frame.
- contributed to making the Makefile for this assignment.
## Steps to execute the file

- Clone the repository
- Change to "Assignment 2" directory

  ```bash
  cd "Assignment 2"
  ```

- Clean the repository of any previous executables and files created from the program using Makefile target `clean`.

  ```bash
  make clean
  ```

- Compile the program using

  ```bash
  make
  ```

- Run the tests using

  ```bash
  make run_test1
  make run_test2
  ```

### Function Descriptions

- #### `RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, const int numPages, ReplacementStrategy strategy, void *stratData)`

  - Initializes the buffer pool.
  - The file is opened and the buffer pool metadata is initialized.

- #### `RC shutdownBufferPool(BM_BufferPool *const bm)`

  - Writes all dirty pages to the disk and clears the buffer pool.
  - Initial checks are added to check for a valid buffer pool, and if it is invalid, `RC_INVALID_BM` error is returned.
  - `forceFlushPool` function is then called to write all the dirty pages to the disk. Error from `forceFlushPool` is returned if the operation fails.
  - We loop over the frames present in the buffer pool and then free each of them.
  - The buffer pool information is then freed as well.
  - All the remaining metadata related to the pool is then nullified as well.

- #### `RC forceFlushPool(BM_BufferPool *const bm)`

  - Writes the dirty pages in the buffer pool to disk.
  - Initial checks are added to check for a valid buffer pool, and if it is invalid, `RC_INVALID_BM` error is returned.
  - Check if the page file is present or not and then throw `RC_FILE_NOT_FOUND` if not present.
  - Loop over the frame list and write dirty pages to the disk. If the write operation fails, we return `RC_WRITE_FAILED` error code;
  - File pointer is then closed to prevent memory leaks.

- #### `RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page)`

  - Marks a page frame dirty.
  - Initial checks are added to check for a valid buffer pool, and if it is invalid, `RC_INVALID_BM` error is returned.
  - The frame is found using `findNodeByPageNum` utility function.
  - If the page is not found, `RC_NON_EXISTING_PAGE_IN_FRAME` is returned, else the page is marked dirty and `RC_OK` is returned.

- #### `RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page)`

  - Finds the page requested and decreases the fixCount if greater than 0.
  - Initial checks are added to check for a valid buffer pool, and if it is invalid, `RC_INVALID_BM` error is returned.
  - The frame is found using `findNodeByPageNum` utility function.
  - We return `RC_NON_EXISTING_PAGE_IN_FRAME` if the page is not found or if it's `fixCount` is less than or equal to 0.
  - If the fixCount of the frame is greater than 0, we decrement it, and return `RC_OK`.

- #### `RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page)`

  - Writes the page content to the page file on disk.
  - Initial checks are added to check for a valid buffer pool, and if it is invalid, `RC_INVALID_BM` error is returned.
  - The frame is found using `findNodeByPageNum` utility function.
  - We return `RC_NON_EXISTING_PAGE_IN_FRAME` if the page is not found.
  - If found, we write the page content to the page file using `writeBlock` function.
  - If the write fails, we return `RC_WRITE_FAILED` error code, else, we increment the number of writes in the page metadata, and return `RC_OK`.

- #### `RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum)`

  - Pins a page in the buffer pool with the buffer management strategy it was initialized with.
  - Initial checks are added to check for a valid buffer pool, and if it is invalid, `RC_INVALID_BM` error is returned.
  - If the value of `pageNum` argument is less than 0, we return `RC_READ_NON_EXISTING_PAGE` error code.
  - Once, the initial checks are done, we call the appropriate function to pin the page for the strategy the buffer pool uses.
  - If the buffer pool strategy is not known, we return `RC_UNKNOWN_STRATEGY` error code.
  - The return code from the `pinPage_{STRATEGY}` is returned, and for additional consistency, we return `RC_OK`.

- #### `PageNumber *getFrameContents (BM_BufferPool *const bm)`

  - Returns the page frames present in the buffer pool.
  - Initial checks are added to check for a valid buffer pool, and if it is invalid, `NULL` is returned.
  - The frameToPage mapping is then returned from the buffer pool metadata.

- #### `bool *getDirtyFlags (BM_BufferPool *const bm)`

  - Get the frames from the pool that are marked dirty.
  - Initial checks are added to check for a valid buffer pool, and if it is invalid, `NULL` is returned.
  - We loop over the frames list in the buffer pool metadata, and mark each frame with TRUE or FALSE in their respective indices in the dirtyFlags array appropriately based on its dirty status.
  - Returns the dirtyFlags array from the buffer pool metadata.

- #### `int *getFixCounts (BM_BufferPool *const bm)`

  - Returns an array of number of fixes each page has.
  - Initial checks are added to check for a valid buffer pool, and if it is invalid, `NULL` is returned.
  - We loop over the frames list in the buffer pool metadata, and set the `fixCount`s of each frame in their respective indices in the fixedCounts array from the frame information.

- #### `int getNumReadIO (BM_BufferPool *const bm)`

  - Returns the number of reads done to a buffer pool.
  - Initial checks are added to check for a valid buffer pool, and if it is invalid, `-1` is returned.
  - The `numRead` value from the buffer pool metadata `bm->mgmtData` is returned.

- #### `int getNumWriteIO (BM_BufferPool *const bm)`

  - Returns the number of reads done to a buffer pool.
  - Initial checks are added to check for a valid buffer pool, and if it is invalid, `-1` is returned.
  - The `numWrite` value from the buffer pool metadata `bm->mgmtData` is returned.

### Page Replacement Strategies

- #### `RC pinPage_FIFO(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum)` - FIFO replacement strategy

  - Pins the given page using FIFO replacement strategy.
  - We first check if the page is present in the buffer pool or not. If present, we return `RC_OK`.
  - We then check for additional space in the frames such that the page can be added to the frame list.
  - If space is available, we traverse the frame list and update the head of the frame list to the last frame.
  - If not, we check for the oldest frame in the list with `fixCount` not equal to 0, and then update the head to that frame.
  - If no frames are found eligible to replace, we return `RC_NO_MORE_SPACE_IN_BUFFER` error code.
  - We then add the new frame to the frame list replacing the old frame, or append it to the list.
  - Any errors are thrown appropriately, and on success, we return `RC_OK`.

- #### `RC pinPage_LRU(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum)` - LRU replacement strategy

  - Pins the given page using LRU replacement strategy.
  - The function first checks if the requested page `pageNum` is already in memory by calling `pageInMemory`.
  - If it is, the head of the frame list is updated to point to this frame, marking it as the most recently used, and returns with `RC_OK`.
  - If the page is not in memory, the function checks if there is space for a new page in the buffer (i.e., `info->numFrames < bm->numPages`). If so, it finds the first free frame starting from the head  and updates its fix count. Otherwise, it looks for an older frame with a fix count of 0 to replace.
  - Once a suitable frame is found, the function calls `updateNewFrame` to update this frame's data with the requested page (`pageNum`). If any errors occur during this process, they are returned immediately.
  - Regardless of whether a new page was loaded or an older one replaced, the head of the frame list is updated to point to the newly accessed frame (either the original page or the replacement frame). This ensures that the most recently used frames are at the front of the list, following the LRU strategy.

- #### LRU-K

  - Pins the given page using LRU-K replacement strategy.
  - The function checks if the requested page (`pageNum`) is already present in the buffer pool's frames using `pageInMemory`. If found, it increments the history of access counts for this page up to a certain "K" value, and returning `RC_OK`.
  - If there is enough space to add a page, we mark the first frame as the head of the list and move to the end of the list.
  - If all frames are filled, the function looks for the least recently used page by iterating through the linked list in reverse order.
  - It specifically searches for a frame with `fixCount == 0`, which means it hasn't been accessed in a while. Among these, it finds the one that is furthest away from being recently used based on its "K" history access count (`info->khist[current->pageNum][K]`), where K is the number of references to each page.
  - If such a page cannot be found, it resorts to finding the first frame with any fixCount and using its age as the tiebreaker.
  - Once the appropriate frame is identified, it updates the frame's status (`updateNewFrame`) and returns `RC_OK` on success.
  - Finally, it updates the "K" history access count for the newly loaded page to keep track of its usage.

- #### LFU

  - Pins the given page using LFU replacement strategy.
  - We first check if the page is present in the buffer pool or not. If present, we update the page's metadata accordingly, and return `RC_OK`.
  - If not in memory, we first check if there is enough space in the buffer to add a new page. If yes, we append the page to the end of the frames list.
  - If there is not enough space, traverse through the frames list and find the least frequently used frame and update that to head.
  - If the head is dirty, write that to the page file on disk.
  - Now, read the page using `readBlock` function and update the frame's metadata accordingly.
  - Update the page-to-frame and frame-to-page mappings as well, accordingly.
  - Return `RC_OK` on success.

- #### CLOCK

  - Pins the given page using CLOCK replacement strategy.
  - We first check if the page is present in the buffer pool or not. If present, we return `RC_OK`.
  - We loop through the frame list and update the frames with the reference bit 1 to 0.
  - If the reference bit is already 0, we choose that frame to replace.
  - If all the frames have the reference bits set, we return `RC_NO_MORE_SPACE_IN_BUFFER`.
  - Else, we use `updateNewFrame` function to update the frame to the new frame.
  - Error codes are returned appropriately.

### Utility functions

- #### `void updateHead(FrameList **list, PageFrameNode *updateNode)`

  - Marks the given node as the head of the frame list.
  - Validations are done such that invalid data is not passed to the function.
  - If the `updateNode` is the tail of the list, we make the previous frame of the `updateNode` as the tail.
  - Else, we remove the links to the previous and next frame from `updateNode`.
  - We then proceed to make the current head as the next frame to the `updateNode`, and the previous frame of the current head as the `updateNode`.
  - We then mark the `updateNode` as the head of the list.

- #### `PageFrameNode *findNodeByPageNum(FrameList *list, const PageNumber pageNum)`

  - Get the frame from the list, if available, by the pageNum.
  - We iterate over the frame list, and return the frame where the frame's pageNum matches the value of `pageNum` passed to the function.
  - If the frame is not found in the frame list, we return `NULL`.

- #### `PageFrameNode *pageInMemory(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum)`

  - Check for the page in memory, mark metadata of the frame accordingly, and return the frame.
  - Initial checks are added to check if the page is present in the memory or not.
  - `findNodeByPageNum` function is used to get the page frame, and if it is not present, we return `NULL`.
  - Appropriate metadata is set for the page and then fixCount is incremented. The frame is also marked as recently used for LRU replacement strategies.
  - The page frame is returned.

- #### `RC updateNewFrame(BM_BufferPool *const bm, PageFrameNode *found, BM_PageHandle *const page, const PageNumber pageNum)`

  - Update a frame in the buffer pool to a new frame.
  - Page file is opened and error from the `openPageFile` function is thrown if any.
  - If the current frame is dirty, we ensure that there is enough space for the frame to be written to disk, and then write the contents using `writeBlock` function.
  - We then nullify the frame in page-to-frame mapping.
  - The new page is read using the `readBlock` function and any error from it is returned.
  - Appropriate metadata is set for the new page frame and the page-to-frame and frame-to-page mapping is updated accordingly.
