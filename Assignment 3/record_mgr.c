#include "record_mgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer_mgr.h"
#include "storage_mgr.h"

// data structure for managing records in a table
typedef struct RecordManager {
    BM_PageHandle *pageHandle;
    BM_BufferPool *bufferPool;
    int sizeOfRecord;
    int numberOfRecords;
    int keySize;
    RID freeSlot; //keeps track of the next free slot in the table
} RecordManager;

//structure for scanning records in a table
typedef struct ScanManager {
    RID recID;  //record ID to scan from
    Expr *cond;     //condition to be evaluated
    int count;  // no of records scaned
    RM_TableData *tbl_data;
    BM_PageHandle pageHandle;
    BM_BufferPool bufferPool;
} ScanManager;

//union to store metadata about the table
typedef union TableMetadata {
    struct {
        char *tableName;
        int numberOfAttributes;
        char **attributeNames;
        DataType *datatypes;
        int typeLength;
        int keySize;
        int *keys;
        RID freeSlot;
    };
    char *metadata;
} TableMetadata;

RecordManager *recordManager;
ScanManager scanManager;
SM_FileHandle *fileHandle;
SM_PageHandle pageHandler;
TableMetadata *metadata;


//converts an integer to a string representation and stores it
void intToStr(int j, int val, char *intStr) {
    int r = 0;
    int q = val;
    int last = j;
    while (q > 0 && j >= 0) {
        r = q % 10;
        q = q / 10;
        intStr[j] = intStr[j] + r;

        j--;
    }
    intStr[last + 1] = '\0';
}


//gets the byte offset of an attribute in the schema based on its position
int attributePositionInSchema(Schema *schema, int positionOfAttribute) {
    int offset = 1;
    while (positionOfAttribute > 0) {
        switch (schema->dataTypes[positionOfAttribute]) {
            case DT_INT:
                offset += sizeof(int);
                break;
            case DT_FLOAT:
                offset += sizeof(float);
                break;
            case DT_BOOL:
                offset += sizeof(bool);
                break;
            case DT_STRING:
                offset +=
                    (sizeof(char) * schema->typeLength[positionOfAttribute]);
                break;
            default:
                break;
        }
        positionOfAttribute--;
    }
    return offset;
}

//finding a free slot in a page based on record size
int freeSlotAt(SM_PageHandle pageData, int recordSize) {
    if (recordSize == 0) {
        return 0;
    }
    int slotsPerPage = PAGE_SIZE / recordSize;
    for (int i = 0; i < slotsPerPage; i++) {
        if (pageData[i * (recordSize)] != '#') {
            return i;
        }
    }
    return -1;
}


//reading the schema from a table's metadata and returns it
Schema *readSchema(char *tableName) {
    fileHandle = (SM_FileHandle *)malloc(sizeof(SM_FileHandle));
    fileHandle->curPagePos = 0;
    fileHandle->fileName = tableName;
    pinPage(recordManager->bufferPool, recordManager->pageHandle, 0);
    pageHandler = (SM_PageHandle)malloc(sizeof(char) * PAGE_SIZE);
    readBlock(0, fileHandle, pageHandler);
    metadata = (TableMetadata *)pageHandler;
    Schema *schema = (Schema *)malloc(sizeof(Schema));
    schema->typeLength = (int *)malloc(sizeof(int));
    schema->numAttr = metadata->numberOfAttributes;
    schema->attrNames = metadata->attributeNames;
    schema->dataTypes = metadata->datatypes;
    memcpy(schema->typeLength, &metadata->typeLength, sizeof(int));
    schema->keySize = metadata->keySize;
    schema->keyAttrs = metadata->keys;
    unpinPage(recordManager->bufferPool, recordManager->pageHandle);
    return schema;
}

// TABLE AND MANAGER

//initializing the record manager and allocates memory for buffer pool and page handle
extern RC initRecordManager(void *mgmtData) {
    recordManager = (RecordManager *)malloc(sizeof(RecordManager));
    recordManager->bufferPool = (BM_BufferPool *)malloc(sizeof(BM_BufferPool));
    recordManager->pageHandle = (BM_PageHandle *)malloc(sizeof(BM_PageHandle));
    recordManager->pageHandle->data =
        (char *)malloc(sizeof(char) * (PAGE_SIZE));
    return RC_OK;
}

//shutting down the record manager and releasing all allocated resources
extern RC shutdownRecordManager() {
    shutdownBufferPool(recordManager->bufferPool);
    free(recordManager->bufferPool);
    free(recordManager->pageHandle);
    return RC_OK;
}


//creating a new table and writing its metadata to the first page
extern RC createTable(char *name, Schema *schema) {
    RC returnCode;
    if (returnCode = createPageFile(name), returnCode != RC_OK) {
        perror("error creating page file for the table");
        return returnCode;
    }
    TableMetadata *metadata = (TableMetadata *)malloc(sizeof(TableMetadata));
    metadata->datatypes =
        (DataType *)malloc(sizeof(DataType) * schema->numAttr);
    metadata->keys = (int *)malloc(sizeof(int) * schema->keySize);
    metadata->tableName = (char *)malloc(sizeof(char) * strlen(name));
    metadata->typeLength = *schema->typeLength;
    memcpy(&metadata->typeLength, schema->typeLength, sizeof(int));
    metadata->keySize = schema->keySize;
    metadata->numberOfAttributes = schema->numAttr;
    metadata->attributeNames =
        (char **)malloc(sizeof(char *) * schema->numAttr);
    for (int i = 0; i < schema->numAttr; i++) {
        metadata->attributeNames[i] =
            (char *)malloc(sizeof(char) * strlen(schema->attrNames[i]));
        memcpy(metadata->attributeNames[i], schema->attrNames[i],
               strlen(schema->attrNames[i]));
    }
    memcpy(metadata->datatypes, schema->dataTypes, schema->numAttr);
    memcpy(metadata->keys, schema->keyAttrs, schema->keySize);
    memcpy(metadata->tableName, name, strlen(name));
    metadata->freeSlot.page = 1;
    metadata->freeSlot.slot = 0;
    recordManager->freeSlot.page = 1;
    recordManager->freeSlot.slot = 0;
    fileHandle = (SM_FileHandle *)malloc(sizeof(SM_FileHandle));
    fileHandle->fileName = name;
    fileHandle->curPagePos = 0;
    fileHandle->totalNumPages = 1;
    if (returnCode = openPageFile(name, fileHandle), returnCode != RC_OK) {
        perror("error opening page file for the table");
        free(metadata);
        return returnCode;
    }
    if (returnCode = ensureCapacity(1, fileHandle), returnCode != RC_OK) {
        perror("error ensuring capacity for the table");
        free(metadata);
        return returnCode;
    }
    if (returnCode = writeBlock(0, fileHandle, (char *)metadata),
        returnCode != RC_OK) {
        perror("error writing metadata to the table");
        free(metadata);
        return returnCode;
    }
    free(metadata);
    return RC_OK;
}

//opening a table and reads its metadata into memory
extern RC openTable(RM_TableData *rel, char *name) {
    initBufferPool(recordManager->bufferPool, name, 3, RS_FIFO, NULL);
    rel->schema = readSchema(name);
    recordManager->numberOfRecords = metadata->numberOfAttributes;
    recordManager->freeSlot = metadata->freeSlot;
    rel->name = name;
    rel->mgmtData = recordManager;
    return RC_OK;
}


//closing the table by releasing the buffer pool
extern RC closeTable(RM_TableData *rel) {
    RecordManager *rManager = rel->mgmtData;
    shutdownBufferPool(rManager->bufferPool);
    return RC_OK;
}


//deleting the table and frees up resources
extern RC deleteTable(char *name) {
    BM_BufferPool *bm = recordManager->bufferPool;
    if (name == NULL) {
        return RC_INVALID_TABLE_NAME;
    }
    RC returnCode = RC_OK;
    if (returnCode = shutdownBufferPool(bm), returnCode != RC_OK) {
        return returnCode;
    };
    if (returnCode = destroyPageFile(name), returnCode != RC_OK) {
        return returnCode;
    }
    return returnCode;
}


//returns the number of tuples (records) in the table
extern int getNumTuples(RM_TableData *rel) {
    return recordManager->numberOfRecords;
}

// HANDLING RECORDS IN TABLE
//inserting a new record into the table at the next free slot
extern RC insertRecord(RM_TableData *rel, Record *record) {
    RecordManager *recordManager = (RecordManager *)rel->mgmtData;
    RID *rid = &record->id;
    char *data, *slotPointer;
    int recordSize = getRecordSize(rel->schema);
    if (pinPage(recordManager->bufferPool, recordManager->pageHandle,
                recordManager->freeSlot.page) != RC_OK) {
        return RC_NO_MORE_SPACE_IN_BUFFER;
    };
    data = recordManager->pageHandle->data;
    int offset = recordManager->freeSlot.slot * recordSize;
    rid->page = recordManager->freeSlot.page;
    rid->slot = recordManager->freeSlot.slot;
    memcpy(data + offset, record->data, recordSize);
    *(data + offset) = '#';
    markDirty(recordManager->bufferPool, recordManager->pageHandle);
    unpinPage(recordManager->bufferPool, recordManager->pageHandle);
    recordManager->numberOfRecords++;
    ++recordManager->freeSlot.slot;
    if (recordManager->freeSlot.slot == PAGE_SIZE / recordSize) {
        recordManager->freeSlot.slot = 0;
        ++recordManager->freeSlot.page;
    }
    pinPage(recordManager->bufferPool, recordManager->pageHandle, 0);
    return RC_OK;
}


//deleting a record from the table by marking its slot as free
extern RC deleteRecord(RM_TableData *rel, RID id) {
    recordManager = (RecordManager *)rel->mgmtData;
    pinPage(recordManager->bufferPool, recordManager->pageHandle, id.page);
    markDirty(recordManager->bufferPool, recordManager->pageHandle);
    char *recordData = recordManager->pageHandle->data +
                       (id.slot * recordManager->sizeOfRecord);
    *recordData = '*';
    unpinPage(recordManager->bufferPool, recordManager->pageHandle);
    return RC_OK;
}


//updating an existing record in the table
extern RC updateRecord(RM_TableData *rel, Record *record) {
    recordManager = (RecordManager *)rel->mgmtData;
    RID id = record->id;
    pinPage(recordManager->bufferPool, recordManager->pageHandle, id.page);
    char *recordData = recordManager->pageHandle->data +
                       (id.slot * recordManager->sizeOfRecord);
    memcpy(recordData, record->data, recordManager->sizeOfRecord);
    markDirty(recordManager->bufferPool, recordManager->pageHandle);
    unpinPage(recordManager->bufferPool, recordManager->pageHandle);
    return RC_OK;
}


//retrieving a record from the table based on the Record ID (RID)
extern RC getRecord(RM_TableData *rel, RID id, Record *record) {
    recordManager = (RecordManager *)rel->mgmtData;
    pinPage(recordManager->bufferPool, recordManager->pageHandle, id.page);
    char *recordData = recordManager->pageHandle->data +
                       (id.slot * recordManager->sizeOfRecord);
    if (*recordData == '*') {
        unpinPage(recordManager->bufferPool, recordManager->pageHandle);
        return RC_SCHEMA_NULL;
    } else {
        record->id = id;
        memcpy(record->data, recordData, recordManager->sizeOfRecord);
    }
    unpinPage(recordManager->bufferPool, recordManager->pageHandle);
    return RC_OK;
}


// SCANS

//initializing a scan on the table with the specified condition
extern RC startScan(RM_TableData *rel, RM_ScanHandle *scan, Expr *cond) {
    BM_BufferPool *bm = recordManager->bufferPool;

    scan->rel = rel;
    scanManager.cond = cond;
    scanManager.recID.page = 1;  //starts scan from the first page
    scanManager.recID.slot = 0;  //starts scan from the first slot
    scanManager.count = 0;      //resets the scanned records count

    scan->mgmtData = &scanManager;

    return RC_OK;

    return RC_OK;
}

//retrieving the next record that meets the scan condition
extern RC next(RM_ScanHandle *scan, Record *record) {
    int pageCapacity = PAGE_SIZE / recordManager->sizeOfRecord;
    int totalTuple = recordManager->numberOfRecords;
    BM_PageHandle *page = recordManager->pageHandle;
    BM_BufferPool *bm = recordManager->bufferPool;
    int curTotalRecScan = scanManager.count;
    int curPageScan = scanManager.recID.page;
    int curSlotScan = scanManager.recID.slot;
    Value *queryExpResult = (Value *)malloc(sizeof(Value));
    scanManager.count = scanManager.count + 1;

    while (curTotalRecScan < totalTuple) {
        scanManager.recID.page = curPageScan;
        scanManager.recID.slot = curSlotScan;

        if (getRecord(scan->rel, scanManager.recID, record) != RC_OK) {
            RC_message = "Record reading failed";
        }
        curTotalRecScan = curTotalRecScan + 1;  // increment counter by 1

        if (scanManager.cond != NULL) {
            evalExpr(record, (scan->rel)->schema, scanManager.cond,
                     &queryExpResult);
            if (queryExpResult->v.boolV == 1) {
                record->id.page = curPageScan;
                record->id.slot = curSlotScan;
                if (curSlotScan == (pageCapacity - 1)) {
                    curPageScan = curPageScan + 1;
                    curSlotScan = 0;
                } else {
                    curSlotScan = curSlotScan + 1;
                }
                scanManager.recID.page = curPageScan;
                scanManager.recID.slot = curSlotScan;
                return RC_OK;
            }
        } else {
            queryExpResult->v.boolV == TRUE;
        }
        if (curSlotScan == (pageCapacity - 1)) {
            curPageScan = curPageScan + 1;
            curSlotScan = 0;
        } else {
            curSlotScan = curSlotScan + 1;
        }
    }
    queryExpResult->v.boolV == TRUE;
    scanManager.recID.page = 1;
    scanManager.recID.slot = 0;
    scanManager.count = 0;
    return RC_RM_NO_MORE_TUPLES;
}


//closing the scan, resetting relevant values
extern RC closeScan(RM_ScanHandle *scan) {
    scanManager.recID.page = 1;
    scanManager.recID.slot = 0;
    scanManager.count = 0;
    return RC_OK;
}

// DEALING WITH SCHEMAS
//getting the size of a single record in the table based on the schema
extern int getRecordSize(Schema *schema) {
    int recordSize = 0;
    for (int i = 0; i < schema->numAttr; i++) {
        switch (schema->dataTypes[i]) {
            case DT_BOOL:
                recordSize += sizeof(bool);
                break;
            case DT_FLOAT:
                recordSize += sizeof(float);
                break;
            case DT_INT:
                recordSize += sizeof(int);
                break;
            case DT_STRING:
                recordSize += (sizeof(char) * *(schema->typeLength));
                break;
            default:
                break;
        }
    }
    return recordSize;
}


//creating a new schema with the specified attributes, datatypes, and keys
extern Schema *createSchema(int numAttr, char **attrNames, DataType *dataTypes,
                            int *typeLength, int keySize, int *keys) {
    Schema *schema = (Schema *)malloc(sizeof(Schema));
    schema->numAttr = numAttr;
    schema->attrNames = attrNames;
    schema->dataTypes = dataTypes;
    schema->typeLength = typeLength;
    schema->keySize = keySize;
    schema->keyAttrs = keys;
    return schema;
}


//frees the memory associated with the schema structure
extern RC freeSchema(Schema *schema) {
    free(schema);
    return RC_OK;
}

//creating a new record with allocated memory for data and initializes its ID
extern RC createRecord(Record **record, Schema *schema) {
    Record *newRecord = (Record *)malloc(sizeof(Record));
    int schemaSize = getRecordSize(schema);
    if (schemaSize == 0) {
        free(newRecord);
        return RC_SCHEMA_NULL;
    }
    newRecord->data = (char *)malloc(schemaSize + 1);
    newRecord->id.page = -1;
    newRecord->id.slot = -1;
    *(++newRecord->data) = '\0';
    *record = newRecord;
    return RC_OK;
}

//frees memory associated with a record
extern RC freeRecord(Record *record) {
    free(record);
    return RC_OK;
}


//retrieving an attribute value from a record based on schema position
extern RC getAttr(Record *record, Schema *schema, int attrNum, Value **value) {
    int offset = attributePositionInSchema(schema, attrNum);
    int sizeOfAttr;
    Value *attribute = (Value *)malloc(sizeof(Value));
    char *valueData;
    switch (schema->dataTypes[attrNum]) {
        case DT_INT: {
            int value = 0;
            memcpy(&value, record->data, sizeof(int));
            attribute->v.intV = value;
            attribute->dt = DT_INT;
            break;
        }
        case DT_FLOAT: {
            float value;
            memcpy(&value, record->data, sizeof(float));
            attribute->v.floatV = value;
            attribute->dt = DT_FLOAT;
            break;
        }
        case DT_BOOL: {
            bool value;
            memcpy(&value, record->data, sizeof(bool));
            attribute->v.boolV = value;
            attribute->dt = DT_BOOL;
            break;
        }
        case DT_STRING:
            sizeOfAttr = sizeof(char) * schema->typeLength[attrNum];
            attribute->v.stringV = (char *)malloc(sizeOfAttr + sizeof(char));
            strncpy(attribute->v.stringV, record->data + offset, sizeOfAttr);
            attribute->v.stringV[sizeOfAttr] = '\0';
            attribute->dt = DT_STRING;
            break;
        default:
            break;
    }
    *value = attribute;
    return RC_OK;
}

//setting an attribute value in a record based on the schema position
extern RC setAttr(Record *record, Schema *schema, int attrNum, Value *value) {
    int offset = attributePositionInSchema(schema, attrNum);
    switch (schema->dataTypes[attrNum]) {
        case DT_INT:
            memcpy(record->data + offset, &value->v.intV, sizeof(int));
            break;
        case DT_FLOAT:
            memcpy(record->data + offset, &value->v.floatV, sizeof(float));
            break;
        case DT_BOOL:
            memcpy(record->data + offset, &value->v.boolV, sizeof(bool));
            break;
        case DT_STRING:
            strncpy(record->data + offset, value->v.stringV,
                    schema->typeLength[attrNum]);
            break;
        default:
            break;
    }
    return RC_OK;
}

