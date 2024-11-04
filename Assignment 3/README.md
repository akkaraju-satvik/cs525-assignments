# CS525 Advanced Database Organization - Assignment 2 - Group 16

> **Group 16 Members**
>
> - Satvik Akkaraju - <sakkaraju@hawk.iit.edu>
> - Manaswini Nagasamudram - <mnagasamudram@hawk.iit.edu>
> - Bindu Bhargavi Palam - <bpalam@hawk.iit.edu>

## Contributions

### Manaswini Nagasamudram
- Implemented functions  in table and manager - initRecordMgr, shutdownRecordManager, createTable, deleteTable, getNumTables
- initRecordManager intializes the record manager by assigning the memory for necessary attributes - buffer pool and page handle.
- shutdownRecordManager - shut downs the record manager by releasing all assigned resources which includes buffer pool  and page handle.
- createTable - creates a new table by initializing the metadata, ensures the file capacity and writes the metadata in the file to the first page. 
- deleteTable - deletes the table by shutting down the buffer pool and destroys the corresponding file.
- getNumTables - which returns the count of records stored in the table.
- Implemented functions in scans - startScan, closeScan
- startScan initializes the table scan by assigning the start page, slot and scan specified condition
- closeScan closes the scan by resetting the scan manager page, slot and the record count

### Satvik Akkaraju
- Implemented functions in table and manager - opentable, closetable
- openTable initializes the buffer pool and loads the schema and metadata in the memory.
- closeTable closes the table by getting the resources used in the buffer pool.
- Implemented functions for scans - next, getRecordSize, createSchema, freeSchema, freeRecord
- next function iterates over the records in the table by applying a scan condition and returns the next record which matches.
- getRecordSize calculates the size of the single record in the table based on the schema attribute types.
- createSchema creates a schema and initializes the schema structure with the specified attributes, data types and primary keys.
- freeSchema releases the memory which is associated with the schema structure.
- freeRecord frees the allocated memory for the particular record.

### Bindu Bhargavi Palam
- Implemented record handling in the table - insert record, delete record, update record, get record
- implemented insertRecord which manages the insertion by calculating the correct slot and after making the page as dirty advance the free slot pointer.
- deleteRecord specifies the record as deleted by assigning the deletion marker
- updateRecord overrides the existing data in the record's slot and marks the particular page as dirty as the page is modified.
- getRecord - gets the record's data and verifies it the page is marked as deleted or not and returns if the slot is empty.
- Implemented methods in dealing schemas as well - createRecord, getAttr, setAttr
- createRecord - function to assign the memory for the new record based on the schema size given and it also initialize its data and fields.
- getAttr - gets the attribute value from a record by calculating the offset based on the schema and its data type.
- setAttr - assigns an attribute value in the record by specifing the correct offset and data type and copies the value into the record data.


## Steps to execute the file

- Clone the repository
- Change to "Assignment 3" directory

  ```bash
  cd "Assignment 3"
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

### initRecordManager
- Initializes the record manager and sets up the environment for the record manager.
- Allocates memory for the buffer pool to store pages and for the page handle that will be used to retrieve data efficiently.


### shutdownRecordManager
- Safely shuts down the record manager by ensuring that all ongoing processes are completed.
- Releases all the allocated resources, including memory allocated for buffer pool and any open table handles, to avoid memory leaks.


### createTable
- Creates a new table in the database with the specified schema.
- This is where it writes the metadata of the table, including all details such as attributes and their respective types of data that would be used in it, into the first page for future reference.


### openTable
- This opens an already existing table by loading the metadata from disk into main memory.
- This readies the table for insertion, updating, or deletion of any record, clearing the table for necessary resources to perform such an action.


### closeTable
- This closes the table, making sure to write all the changes back to disk.
- Frees the buffer pool and related resources; henceforth, the table becomes unavailable, pending reopening.


### deleteTable
- Removes the named table from the database; all related records will be lost.
- The method cleans up resources, such as memory or disk space, and assures that no residual data are left behind.


### getNumTuples
- Returns the total number of records (tuples) currently residing in the table.
- Convenient to test the size of the table, and operations relying on the amount of records, like pagination. 8. insertRecord Adds a new record into the table at the next free slot. Ensures data integrity; data will be checked against the schema before insertion into the table. 9. deleteRecord Deletes a record by marking its slot as free, making the slot available to insertions in the future.
- It maintains the table in a consistent state by implementing those essential metadata changes that reflect its deletion.


### updateRecord
- Updates an already existing record with new values using its Record ID (RID).
- New values are checked against the schema to check for consistency and integrity of the data.


### getRecord
- Returns a record using its Record ID (RID), which in turn retrieves the data making up the record itself.
- Handles the result in case the specified RID may not exist; in such cases, return an error.


### startScan
- Starts a scan for the table, ensuring that records meeting some conditions are returned.
- Sets up the parameters necessary for the scan, including the condition each record must satisfy.


### next
- Returns the next record satisfying the scan condition to iterate through qualifying records.
- Returns status of whether it found a record or the end of the scan was reached.


### closeScan
- Closes the current scan and cleans up resources used by the scan.
- It should reset relevant values so that new scans would start clean and not interfere with previous results.


### getRecordSize
- Returns length of one record, useful for memory allocation based on schema.
- It helps calculate the total memory footprint of the table by multiplying the record size times the number of tuples.


### createSchema
- Creates a new schema with specified attributes, data types, and primary/foreign keys.
- It keeps metadata about the schema for future reference while creating or managing tables.


### freeSchema
- Sets a schema free and returns the memory allocated with it so that no memory leaks occur once the schemas are not needed anymore.
- Resets the schema pointer to null after freeing. It is good practice against the so-called dangling pointers.


### createRecord
- Creates a new record and allocates memory for its data according to a given schema.
- Initializes the ID of the record and sets it ready for insertion into the table.


### freeRecord
- Frees the memory associated with a record. This assures that there are no memory leaks when any records are deleted or otherwise no longer needed.
- Resets the record pointer to null to avoid dangling pointers after freeing.


### getAttr
- Returns the value of an attribute of a record given its schema position.
- It performs type conversions  to return a value in correct type.


### setAttr
- Assigns an attribute value to a record, given its schema position.
- it checks the new value against the schema for proper data integrity and constraints.