#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "record_mgr.h"
#include "buffer_mgr.h"
#include "storage_mgr.h"


typedef struct RecordManager
{
    
    BM_PageHandle pageHandle;   // Buffer Manager PageHandle 
    // Buffer Manager's Buffer Pool for using Buffer Manager    
    BM_BufferPool bufferPool;
    RID record_id;
    // This variable defines the condition for scanning the records in the table
    Expr *condition;
    // This variable stores the total number of tuples in the table
    int tuples_count;
    // This variable stores the location of first free page which has empty slots in table
    int freePage;
    // This variable stores the count of the number of records scanned
    int scan_count;
} RecordManager;

const int MAX_NUMBER_OF_PAGES = 100;
const int ATTRIBUTE_SIZE = 15; 

RecordManager *recordManager;


// This function returns a free slot within a page

int FreeSlot(const char *data, int recordSize) {
    int total_slots = PAGE_SIZE / recordSize;
    int i = 0;

    while (i < total_slots) {
        const char *record = &data[i * recordSize];

        if (*record != '+') {
            return i;
        }
        i++;
    }
    return -1;
}

// This function initializes the Record Manager
extern RC initRecordManager (void *mgmtData)
{
    // Initiliazing Storage Manager
    initStorageManager();
    return RC_OK;
}
// This functions shuts down the Record Manager
extern RC shutdownRecordManager ()
{
    recordManager = NULL;
    free(recordManager);
    return RC_OK;
}

extern RC openTable(RM_TableData *rel, char *name)
{
    SM_PageHandle pageHandle;
    int attributeCount, i = 0;

    rel->mgmtData = recordManager;
    rel->name = name;

    // Pin the first page to the buffer pool
    pinPage(&recordManager->bufferPool, &recordManager->pageHandle, 0);

    // Set pageHandle to point to the first page's data
    pageHandle = (char*) recordManager->pageHandle.data;

    // Extract tuples_count and freePage information from the page
    recordManager->tuples_count = *(int*) pageHandle;
    pageHandle += sizeof(int);
    recordManager->freePage = *(int*) pageHandle;
    pageHandle += sizeof(int);
    attributeCount = *(int*) pageHandle;
    pageHandle += sizeof(int);

    // Allocate memory for schema and its attributes
    Schema *schema = (Schema*) malloc(sizeof(Schema));
    schema->numAttr = attributeCount;
    schema->attrNames = (char**) malloc(sizeof(char*) * attributeCount);
    schema->dataTypes = (DataType*) malloc(sizeof(DataType) * attributeCount);
    schema->typeLength = (int*) malloc(sizeof(int) * attributeCount);

    // Use while loop to load attribute names, data types, and type lengths
    while (i < attributeCount) {
        schema->attrNames[i] = (char*) malloc(ATTRIBUTE_SIZE);
        strncpy(schema->attrNames[i], pageHandle, ATTRIBUTE_SIZE);
        pageHandle += ATTRIBUTE_SIZE;

        schema->dataTypes[i] = *(int*) pageHandle;
        pageHandle += sizeof(int);

        schema->typeLength[i] = *(int*) pageHandle;
        pageHandle += sizeof(int);

        i++;
    }

    // Assign the schema to the table
    rel->schema = schema;

    // Unpin the page and write it back to disk
    unpinPage(&recordManager->bufferPool, &recordManager->pageHandle);
    forcePage(&recordManager->bufferPool, &recordManager->pageHandle);

    return RC_OK;
}


extern RC createTable(char *name, Schema *schema) 
{
    int result;
    char data[PAGE_SIZE];
    char *pageHandle = data;

    // Allocate memory for the RecordManager
    recordManager = (RecordManager*) malloc(sizeof(RecordManager)); 

    // Initialize the Buffer Pool using the LRU page replacement policy
    initBufferPool(&recordManager->bufferPool, name, MAX_NUMBER_OF_PAGES, RS_LRU, NULL);

    // Initialize metadata for the table: number of tuples, first free page, and schema attributes
    *(int*)pageHandle = 0; // Set number of tuples to 0
    pageHandle += sizeof(int);
    
    *(int*)pageHandle = 1; // First free page (page 1)
    pageHandle += sizeof(int);
    
    *(int*)pageHandle = schema->numAttr; // Set the number of attributes
    pageHandle += sizeof(int);
    
    *(int*)pageHandle = schema->keySize; // Set key size
    pageHandle += sizeof(int);

    // Store schema attributes: attribute names, data types, and lengths
    for (int i = 0; i < schema->numAttr; i++) {
        // Copy attribute name to the page
        strncpy(pageHandle, schema->attrNames[i], ATTRIBUTE_SIZE);
        pageHandle += ATTRIBUTE_SIZE;
        
        // Store attribute data type
        *(int*)pageHandle = (int)schema->dataTypes[i];
        pageHandle += sizeof(int);
        
        // Store attribute type length
        *(int*)pageHandle = schema->typeLength[i];
        pageHandle += sizeof(int);
    }

    // Create a file for the table and handle errors
    SM_FileHandle fileHandle;
    
    if ((result = createPageFile(name)) != RC_OK) 
        return result;

    if ((result = openPageFile(name, &fileHandle)) != RC_OK) 
        return result;

    // Write the schema metadata to the first block of the file
    if ((result = writeBlock(0, &fileHandle, data)) != RC_OK) 
        return result;

    // Close the file after writing the schema
    if ((result = closePageFile(&fileHandle)) != RC_OK) 
        return result;

    return RC_OK;
}

extern RC closeTable (RM_TableData *rel)
{
    // Storing the Table's meta data
    RecordManager *recordManager = rel->mgmtData;
    
    // Shutting down Buffer Pool    
    shutdownBufferPool(&recordManager->bufferPool);
    //rel->mgmtData = NULL;
    return RC_OK;
}

// This function deletes the table having table name "name"
extern RC deleteTable (char *name)
{
    destroyPageFile(name);
    return RC_OK;
}

// This function returns the number of tuples (records) in the table referenced by "rel"
extern int getNumTuples (RM_TableData *rel)
{
    RecordManager *recordManager = rel->mgmtData;
    return recordManager->tuples_count;
}



// This function deletes a record having Record ID "id" in the table referenced by "rel"

extern RC deleteRecord(RM_TableData *rel, RID id)
{
    // Access the RecordManager metadata
    RecordManager *recordManager = rel->mgmtData;

    // Pin the page containing the record to be deleted
    pinPage(&recordManager->bufferPool, &recordManager->pageHandle, id.page);

    // Update the free page reference to this page
    recordManager->freePage = id.page;
    char *data = recordManager->pageHandle.data;
    int recordSize = getRecordSize(rel->schema);
    data += (id.slot * recordSize);
    *data = '-';

    // Mark the page as dirty since it has been modified
    markDirty(&recordManager->bufferPool, &recordManager->pageHandle);

    // Unpin the page once the deletion is complete
    unpinPage(&recordManager->bufferPool, &recordManager->pageHandle);

    return RC_OK;
}


// This function updates a record referenced by "record" in the table referenced by "rel"

extern RC updateRecord (RM_TableData *rel, Record *record)
{
    // Access RecordManager from the table's metadata
    RecordManager *recordManager = rel->mgmtData;

    // Retrieve record's page and pin it to memory
    RID recordID = record->id;
    pinPage(&recordManager->bufferPool, &recordManager->pageHandle, recordID.page);

    // Calculate record's size and its location in the page
    int recordSize = getRecordSize(rel->schema);
    char *recordDataPointer = recordManager->pageHandle.data + (recordID.slot * recordSize);

    // Mark record as valid (using tombstone mechanism) by setting the first byte to '+'
    recordDataPointer[0] = '+';

    // Copy the updated record data, excluding the tombstone marker
    memcpy(recordDataPointer + 1, record->data + 1, recordSize - 1);

    // Mark the page as dirty since we made changes
    markDirty(&recordManager->bufferPool, &recordManager->pageHandle);

    // Unpin the page after the update is complete
    unpinPage(&recordManager->bufferPool, &recordManager->pageHandle);

    return RC_OK;
}




// This function retrieves a record having Record ID "id" in the table referenced by "rel".
// The result record is stored in the location referenced by "record"

extern RC getRecord (RM_TableData *rel, RID id, Record *record)
{
    // Accessing the record manager metadata stored in the table structure
    RecordManager *recordManager = rel->mgmtData;

    // Pinning the page in the buffer pool where the desired record is located
    pinPage(&recordManager->bufferPool, &recordManager->pageHandle, id.page);

    int recordSize = getRecordSize(rel->schema);
    char *dataPointer = recordManager->pageHandle.data + (id.slot * recordSize);
    
    if (*dataPointer != '+')
    {
        return RC_RM_NO_TUPLE_WITH_GIVEN_RID;
    }
    record->id = id;
    memcpy(record->data + 1, dataPointer + 1, recordSize - 1);

    // Unpin the page as it's no longer needed in memory
    unpinPage(&recordManager->bufferPool, &recordManager->pageHandle);

    return RC_OK;
}



// This function scans all the records using the condition
extern RC startScan (RM_TableData *rel, RM_ScanHandle *scan, Expr *cond)
{
    // Ensure the scan condition (expression) is provided
    if (cond == NULL)
    {
        return RC_SCAN_CONDITION_NOT_FOUND;
    }

    openTable(rel, "ScanTable");

    RecordManager *scanManager = (RecordManager*) malloc(sizeof(RecordManager));

    scan->mgmtData = scanManager;
    scanManager->record_id.page = 1; 
    scanManager->record_id.slot = 0;    
    scanManager->scan_count = 0;         
    scanManager->condition = cond;      

    RecordManager *tableManager = rel->mgmtData;

    tableManager->tuples_count = ATTRIBUTE_SIZE;

    scan->rel = rel;

    return RC_OK;
}

// This function inserts a new record in the table referenced by "rel" and updates the 'record' parameter with the Record ID of he newly inserted record
extern RC insertRecord (RM_TableData *rel, Record *record)
{

    RecordManager *recordManager = rel->mgmtData;   
    
    RID *record_id = &record->id; 
    
    char *data, *slotPointer;
    
    // Getting the size in bytes needed to store on record for the given schema
    int recordSize = getRecordSize(rel->schema);
    
    // Setting first free page to the current page
    record_id->page = recordManager->freePage;

    // Pinning page i.e. telling Buffer Manager that we are using this page
    pinPage(&recordManager->bufferPool, &recordManager->pageHandle, record_id->page);
    
    // Setting the data to initial position of record's data
    data = recordManager->pageHandle.data;
    
    // Getting a free slot using our custom function
    record_id->slot = FreeSlot(data, recordSize);

    while(record_id->slot == -1)
    {
        // If the pinned page doesn't have a free slot then unpin that page
        unpinPage(&recordManager->bufferPool, &recordManager->pageHandle);  
        
        record_id->page++;
        
        // Bring the new page into the BUffer Pool using Buffer Manager
        pinPage(&recordManager->bufferPool, &recordManager->pageHandle, record_id->page);
           
        data = recordManager->pageHandle.data;

        record_id->slot = FreeSlot(data, recordSize);
    }
    
    slotPointer = data;
    
    // Mark page dirty to notify that this page was modified
    markDirty(&recordManager->bufferPool, &recordManager->pageHandle);
    slotPointer = slotPointer + (record_id->slot * recordSize);

    *slotPointer = '+';
    // Copy the record's data to the memory location pointed by slotPointer
    memcpy(++slotPointer, record->data + 1, recordSize - 1);

    // Unpinning a page i.e. removing a page from the BUffer Pool
    unpinPage(&recordManager->bufferPool, &recordManager->pageHandle);
    
    // Incrementing count of tuples
    recordManager->tuples_count++;
    
    // Pinback the page 
    pinPage(&recordManager->bufferPool, &recordManager->pageHandle, 0);

    return RC_OK;
}

// This function creates a new schema
extern Schema *createSchema (int numAttr, char **attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys)
{
    // Allocate memory space to schema
    Schema *schema = (Schema *) malloc(sizeof(Schema));
    // Set the Number of Attributes in the new schema   
    schema->numAttr = numAttr;
    // Set the Attribute Names in the new schema
    schema->attrNames = attrNames;
    // Set the Data Type of the Attributes in the new schema
    schema->dataTypes = dataTypes;
    // Set the Type Length of the Attributes i.e. STRING size  in the new schema
    schema->typeLength = typeLength;
    // Set the Key Size  in the new schema
    schema->keySize = keySize;
    // Set the Key Attributes  in the new schema
    schema->keyAttrs = keys;

    return schema; 
}

// This function removes a schema from memory and de-allocates all the memory space allocated to the schema.
extern RC freeSchema (Schema *schema)
{
    // De-allocating memory space occupied by 'schema'
    free(schema);
    return RC_OK;
}

extern RC next (RM_ScanHandle *scan, Record *record)
{
    // Initialize scan and table managers, and schema
    RecordManager *scanManager = scan->mgmtData;
    RecordManager *tableManager = scan->rel->mgmtData;
    Schema *schema = scan->rel->schema;
    
    if (scanManager->condition == NULL)
        return RC_SCAN_CONDITION_NOT_FOUND;

    Value *result = (Value *) malloc(sizeof(Value));
    
    // Calculate record size and total slots per page
    int recordSize = getRecordSize(schema);
    int totalSlots = PAGE_SIZE / recordSize;

    // Get current scan count and tuple count
    int scanCount = scanManager->scan_count;
    int tuplesCount = tableManager->tuples_count;

    // Check if table has no tuples
    if (tuplesCount == 0)
        return RC_RM_NO_MORE_TUPLES;

   
    while (scanCount <= tuplesCount)
    {
        // Initialize record ID if scan is just starting
        if (scanCount == 0)
        {
            scanManager->record_id.page = 1;
            scanManager->record_id.slot = 0;
        }
        else
        {
            scanManager->record_id.slot++;

            // Move to the next page if current page slots are fully scanned
            if (scanManager->record_id.slot >= totalSlots)
            {
                scanManager->record_id.slot = 0;
                scanManager->record_id.page++;
            }
        }

        // Pin the page into buffer pool
        pinPage(&tableManager->bufferPool, &scanManager->pageHandle, scanManager->record_id.page);
        
        // Fetch the data from the current slot
        char *data = scanManager->pageHandle.data + (scanManager->record_id.slot * recordSize);
        
        // Update the record's page and slot
        record->id.page = scanManager->record_id.page;
        record->id.slot = scanManager->record_id.slot;
        char *dataPointer = record->data;
        *dataPointer = '-';
        memcpy(++dataPointer, data + 1, recordSize - 1);

        scanManager->scan_count++;
        scanCount++;
        evalExpr(record, schema, scanManager->condition, &result);

        // If the condition is satisfied, unpin the page and return success
        if (result->v.boolV == TRUE)
        {
            unpinPage(&tableManager->bufferPool, &scanManager->pageHandle);
            return RC_OK;
        }
    }
    
    // Unpin the page and reset the scan manager
    unpinPage(&tableManager->bufferPool, &scanManager->pageHandle);
    scanManager->record_id.page = 1;
    scanManager->record_id.slot = 0;
    scanManager->scan_count = 0;

    // Return no more tuples if none satisfy the condition
    return RC_RM_NO_MORE_TUPLES;
}



// This function closes the scan operation.
extern RC closeScan (RM_ScanHandle *scan)
{
    RecordManager *scanManager = scan->mgmtData;
    RecordManager *recordManager = scan->rel->mgmtData;

  
    if(scanManager->scan_count > 0)
    {
        // Unpin the page 
        unpinPage(&recordManager->bufferPool, &scanManager->pageHandle);
        
        // Reset the Scan Manager's values
        scanManager->scan_count = 0;
        scanManager->record_id.page = 1;
        scanManager->record_id.slot = 0;
    }
        scan->mgmtData = NULL;
        free(scan->mgmtData);  
    
    return RC_OK;
}

// This function returns the record size of the schema referenced by "schema"

extern int getRecordSize(Schema *schema) {
    int size = 0;

    // Iterate through all the attributes in the schema
    for (int i = 0; i < schema->numAttr; i++) {
        // Determine the size based on the data type of the attribute
        if (schema->dataTypes[i] == DT_STRING) {
            // If the attribute is STRING, add the defined length
            size += schema->typeLength[i];
        } else if (schema->dataTypes[i] == DT_INT) {
            // If the attribute is INTEGER, add the size of INT
            size += sizeof(int);
        } else if (schema->dataTypes[i] == DT_FLOAT) {
            // If the attribute is FLOAT, add the size of FLOAT
            size += sizeof(float);
        } else if (schema->dataTypes[i] == DT_BOOL) {
            // If the attribute is BOOLEAN, add the size of BOOLEAN
            size += sizeof(bool);
        }
    }

    return size + 1; // Return total size with an additional byte for null-termination
}

// This function sets the offset (in bytes) from initial position to the specified attribute of the record into the 'result' parameter passed through the function


RC attrOffset(Schema *schema, int attrNum, int *result) {
    int i;
    *result = 1; // Initialize result to 1 for the initial offset

   
    for (i = 0; i < attrNum; i++) {
        // Check the data type of the attribute using if-else statements
        if (schema->dataTypes[i] == DT_STRING) {
            // If the attribute is STRING, add its defined length
            *result += schema->typeLength[i];
        } else if (schema->dataTypes[i] == DT_INT) {
            // If the attribute is INTEGER, add size of INT
            *result += sizeof(int);
        } else if (schema->dataTypes[i] == DT_FLOAT) {
            // If the attribute is FLOAT, add size of FLOAT
            *result += sizeof(float);
        } else if (schema->dataTypes[i] == DT_BOOL) {
            // If the attribute is BOOLEAN, add size of BOOLEAN
            *result += sizeof(bool);
        }
    }

    return RC_OK; // Return success code
}


// This function removes the record from the memory.
extern RC freeRecord (Record *record)
{
    // De-allocating memory space allocated to record and freeing up that space
    free(record);
    return RC_OK;
}


// This function creates a new record in the schema referenced by "schema"

extern RC createRecord(Record **record, Schema *schema) {
   
    Record *new_record = (Record*) malloc(sizeof(Record));
    if (new_record == NULL) {
        return RC_ERROR; // Handle memory allocation failure
    }

    // Retrieve the size of the record based on the schema
    int recordSize = getRecordSize(schema);
    
    // Allocate memory for the record's data
    new_record->data = (char*) malloc(recordSize);
    if (new_record->data == NULL) {
        free(new_record); // Free previously allocated memory
        return RC_ERROR; // Handle memory allocation failure
    }

    // Initialize the record ID's page and slot to -1 
    new_record->id.page = -1;
    new_record->id.slot = -1;

    new_record->data[0] = '-'; 
    new_record->data[1] = '\0'; 

    // Assign the newly created record to the output parameter
    *record = new_record;

    return RC_OK; // Return success code
}



// This function retrieves an attribute from the given record in the specified schema
extern RC getAttr(Record *record, Schema *schema, int attrNum, Value **value) {
    int offset = 0;
    char *dataPointer = NULL;
    Value *attribute = NULL;

    // Get the attribute's offset in the record
    attrOffset(schema, attrNum, &offset);

    // Allocate memory for the Value structure
    attribute = (Value*) malloc(sizeof(Value));
    if (attribute == NULL) {
        return RC_ERROR; // Handle memory allocation failure
    }

    // Get the pointer to the attribute's data in the record
    dataPointer = record->data + offset;

    // Determine the attribute's data type and handle accordingly
    switch (schema->dataTypes[attrNum]) {
        case DT_STRING: {
            // Handle STRING attribute
            int length = schema->typeLength[attrNum];
            attribute->v.stringV = (char *) malloc(length + 1); // Allocate space for the string
            if (attribute->v.stringV == NULL) {
                free(attribute); // Free previously allocated memory
                return RC_ERROR; // Handle memory allocation failure
            }
            strncpy(attribute->v.stringV, dataPointer, length);
            attribute->v.stringV[length] = '\0'; // Null-terminate the string
            attribute->dt = DT_STRING;
            break;
        }

        case DT_INT: {
            // Handle INTEGER attribute
            memcpy(&attribute->v.intV, dataPointer, sizeof(int));
            attribute->dt = DT_INT;
            break;
        }

        case DT_FLOAT: {
            // Handle FLOAT attribute
            memcpy(&attribute->v.floatV, dataPointer, sizeof(float));
            attribute->dt = DT_FLOAT;
            break;
        }

        case DT_BOOL: {
            // Handle BOOLEAN attribute
            memcpy(&attribute->v.boolV, dataPointer, sizeof(bool));
            attribute->dt = DT_BOOL;
            break;
        }

        default: {
            // Handle unsupported data types
            printf("Serializer not defined for the given datatype.\n");
            free(attribute);
            return RC_ERROR;
        }
    }

    // Assign the result to the output parameter
    *value = attribute;

    return RC_OK;
}



// This function sets the attribute value in the record in the specified schema

extern RC setAttr(Record *record, Schema *schema, int attrNum, Value *value) {
    int offset = 0;

    // Get the offset value of attributes based on the attribute number
    attrOffset(schema, attrNum, &offset);

   
    char *dataPointer = record->data + offset;

    // Set the attribute value based on its data type using if-else statements
    if (schema->dataTypes[attrNum] == DT_STRING) {
        // Handle STRING attribute
        int length = schema->typeLength[attrNum];
        strncpy(dataPointer, value->v.stringV, length); // Copy string value
        // No need to update dataPointer since we're not using it further

    } else if (schema->dataTypes[attrNum] == DT_INT) {
        // Handle INTEGER attribute
        *(int *)dataPointer = value->v.intV;

    } else if (schema->dataTypes[attrNum] == DT_FLOAT) {
        // Handle FLOAT attribute
        *(float *)dataPointer = value->v.floatV;

    } else if (schema->dataTypes[attrNum] == DT_BOOL) {
        // Handle BOOLEAN attribute
        *(bool *)dataPointer = value->v.boolV;

    } else {
        // Handle unsupported data types
        printf("Serializer not defined for the given datatype.\n");
        return RC_ERROR; 
    }

    return RC_OK;
}

