# CS525 Advanced Database Organization - Assignment 4 - Group 16

> **Group 16 Members**
>
> - Satvik Akkaraju - <sakkaraju@hawk.iit.edu>
> - Manaswini Nagasamudram - <mnagasamudram@hawk.iit.edu>
> - Bindu Bhargavi Palam - <bpalam@hawk.iit.edu>

## Contributions

### Manaswini Nagasamudram

- Implemented core B+ tree operations: `initIndexManager`, `shutdownIndexManager`, `createBtree`, `openBtree`, `closeBtree`, `deleteBtree`
- Implemented utility functions: `getNumNodes`, `getNumEntries`, `getKeyType`
- initIndexManager initializes the storage manager for B-tree operations.
- shutdownIndexManager releases all resources and resets the tree manager.
- createBtree creates a new B-tree by initializing the tree manager, allocating memory, and creating a page file.
- openBtree initializes the BTreeHandle and sets up the buffer pool for the B-tree.
- closeBtree marks the page as dirty, shuts down the buffer pool, and frees allocated memory.
- deleteBtree removes the page file storing the B-tree.
- getNumNodes retrieves the number of nodes from the tree manager.
- getNumEntries retrieves the number of entries from the tree manager.
- getKeyType retrieves the key type from the tree manager

### Satvik Akkaraju

- Implemented key operations: `findKey`, `insertKey`, `deleteKey`
- Implemented utility functions: `createNode`, `createLeaf`, `findLeaf`, `makeRecord`
- findKey traverses the B-tree to locate a specific key and returns its associated RID.
- insertKey handles the insertion of a new key-value pair into the B-tree, including node splitting if necessary.
- deleteKey removes a key from the B-tree and adjusts the tree structure accordingly.
- createNode allocates memory for a new internal node and initializes its properties.
- createLeaf creates a new leaf node for the B-tree.
- findLeaf traverses the B-tree to locate the leaf node where a key should be inserted or found.
- makeRecord creates a new NodeData structure to store a record's RID.

### Bindu Bhargavi Palam

- Implemented tree scanning operations: `openTreeScan`, `nextEntry`, `closeTreeScan`, `printTree`
- Implemented node manipulation functions: `insertIntoLeaf`, `insertIntoLeafAfterSplitting`, `insertIntoNode`, `insertIntoNodeAfterSplitting`, `redistributeNodes`, `mergeNodes`
- openTreeScan initializes the scan manager and sets up the initial scan position.
- nextEntry retrieves the next entry in the B-tree during a scan operation.
- closeTreeScan frees the memory allocated for the scan manager and resets scan-related metadata.
- insertIntoLeaf inserts a new key-value pair into a leaf node with available space.
- insertIntoLeafAfterSplitting handles insertion into a full leaf node by splitting it.
- insertIntoNode inserts a new key and pointer into an internal node with available space.
- insertIntoNodeAfterSplitting manages insertion into a full internal node by splitting it.
- printTree traverses the B-tree in a breadth-first manner and prints node information for debugging.
- redistributeNodes balances entries between two nodes after a deletion operation.
- mergeNodes combines two under-full nodes into a single node during deletion

## Steps to execute the file

- Clone the repository
- Change to "Assignment 4" directory

  ```bash
  cd "Assignment 4"
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
  make run
  make run_expr
  ```

### Function Descriptions

### initIndexManager

- Initializes storage manager and index management for the B+ tree.
- Allocates memory for other managers to store relevant metadata for their respective operations.

### shutdownIndexManager

- Shutdown the index manager by deallocating the memories and nullifying the variables.
- Returns RC_OK on success.

### createBtree

- Get the maximum number of nodes possible for the B+ Tree and return `RC_ORDER_TOO_HIGH_FOR_PAGE` if the required number of nodes is greater than max possible nodes.
- Then allocate the required memories for the tree manager and the buffer pool and creates a page file to store the B+ tree.

### openBtree

- Allocate memory to the passed in BTreeHandle and point the management data to the tree manager.
- Initialize a buffer pool if not yet initialized, and return RC_OK on successful initialization.

### closeBtree

- Write the data mark the page pointed by the tree manager as dirty and shutdown the buffer pool gracefully.
- Deallocate the memories as required.

### deleteBtree

- Deletes the page file storing the B+ tree.

### getNumNodes

- Get the number of nodes from the tree manager.

### getNumEntries

- Get the number of entries from the tree manager.

### getKeyType

- Get the key type from the tree manager.

### findKey

- Traverse through the B+ tree to the leaf nodes and find the required key.
- Store the RID in `result` variable and return RC_OK;
- Return `RC_IM_KEY_NOT_FOUND` if not found.

### insertKey

- Checks if the key already exists in the tree and returns `RC_IM_KEY_ALREADY_EXISTS` if it exists.
- Else traverse through the B+ tree to find the appropriate position and proper redistribution of keys and insert the key into the leaf nodes.
- Returns `RC_OK`.

### deleteKey

- Deletes the key from the B+ tree and adjusts the B+ tree accordingly by redistributing keys (merges and splits) appropriately.

### openTreeScan

- Allocates memory for the scan manager.
- Initialize the scanner metadata values.

### nextEntry

- Retrieves the next entry in the B+ tree during a scan operation.
- Returns the RID of the next entry if available, or RC_IM_NO_MORE_ENTRIES if the scan is complete.

### closeTreeScan

- Frees the memory allocated for the scan manager.
- Resets any scan-related metadata.

### printTree

- Traverses the B+ tree in a breadth-first manner.
- Prints the keys and associated information for each node.
- Useful for debugging and visualizing the tree structure.

## Utility functions

### createLeaf

- Creates a new leaf node for the B+ tree.
- Initializes the node's properties and allocates necessary memory.

### createNode

- Creates a new internal node for the B+ tree.
- Allocates memory and initializes node properties.

### findLeaf

- Traverses the B+ tree to find the leaf node where a given key should be located.
- Used in various operations like insertion and deletion.

### insertIntoLeaf

- Inserts a new key-value pair into a leaf node.
- Handles the case where the leaf has space for the new entry.

### insertIntoLeafAfterSplitting

- Handles insertion into a leaf node when it's full.
- Splits the leaf node and redistributes entries.

### insertIntoNode

- Inserts a new key and pointer into an internal node.
- Used when the node has space for the new entry.

### insertIntoNodeAfterSplitting

- Handles insertion into an internal node when it's full.
- Splits the node and redistributes entries and pointers.

### insertIntoParent

- Inserts a new key and pointer into the parent node after a node split.
- May recursively split parent nodes if necessary.

### deleteEntry

- Removes a key-value pair from a node.
- Handles redistribution or merging of nodes if necessary after deletion.

### redistributeNodes

- Redistributes entries between two nodes to maintain balance after a deletion.
- Used to avoid merging nodes when possible.

### mergeNodes

- Combines two under-full nodes into a single node.
- Used when redistribution is not possible during deletion.

### isLess

- Compares two Value pointers to determine if the first is less than the second.
- Handles different data types: integers, floats, strings, and booleans.
- Returns true if key1 is less than key2, false otherwise.

### isGreater

- Compares two Value pointers to determine if the first is greater than the second.
- Supports integers, floats, strings, and booleans.
- Returns true if key1 is greater than key2, false otherwise.

### isEqual

- Checks if two Value pointers are equal.
- Compares integers, floats, strings, and booleans.
- Returns true if key1 is equal to key2, false otherwise.

### enqueue

- Adds a new node to the end of a queue in the BTreeManager.
- If the queue is empty, sets the new node as the queue's head.
- Otherwise, traverses the queue to add the new node at the end

### dequeue

- Removes and returns the first node from the queue in the BTreeManager.
- Updates the queue's head to the next node.
- Returns the dequeued node with its 'next' pointer set to NULL.

### path_to_root

- Calculates the length of the path from a given child node to the root of the B+ tree.
- Traverses up the tree using parent pointers, incrementing the length counter.
- Returns the number of nodes between the child and the root, inclusive.

### makeRecord

- Creates a new NodeData structure to store a record's RID.
- Allocates memory for the NodeData structure.
- Copies the provided RID into the new structure.
- Returns NULL if memory allocation fails or if the input RID is NULL.
