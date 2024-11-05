#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "record_mgr.h"
#include "buffer_mgr.h"
#include "storage_mgr.h"

typedef struct RecordManager {
    BM_PageHandle pageHandle;  
    BM_BufferPool bufferPool;
    RID recordID;
    Expr *cond;
    int tupleCount;
    int freePage;
    int scanCount;
} RecordManager;

RecordManager *recordManager;

const int TOTAL_MAX_NO_OF_PAGES = 100;
const int SIZE_OF_ATTR = 15; 

//to return a free slot within a page
int FreeSlot(const char *data, int recordSize) {
    for(int i = 0; i < PAGE_SIZE / recordSize; i++) {
        const char  *record = &data[i * recordSize];
        if (!(*record == '+'))
            return i;
    }
    return -1;
}


//to initialize the Record Manager
extern RC initRecordManager(void *mgmtData) {
    printf("Initiating Record Manager........ ");
    initStorageManager();
    return RC_OK;
}


//to shut down the Record Manager
extern RC shutdownRecordManager() {
    free(recordManager);
    recordManager = NULL;
    return RC_OK;
}

//to create a new table by initializing the metadata, ensures the file capacity and writes the
// metadata in the file to the first page. 
extern RC createTable(char *name, Schema *schema) {
    char data[PAGE_SIZE];
    char *ph = data;
    recordManager = malloc(sizeof(*recordManager)); 

    initBufferPool(&recordManager->bufferPool, name, TOTAL_MAX_NO_OF_PAGES, RS_LRU, NULL);
    *(int*)ph = 0; 
    ph += 4;
    
    *(int*)ph = 1; 
    ph += 4;
    
    *(int*)ph = schema->numAttr;
    ph += 4;
    
    *(int*)ph = schema->keySize; 
    ph += 4;

    for (int i = 0; i < schema->numAttr; i++) {
        strncpy(ph, schema->attrNames[i], SIZE_OF_ATTR);
        ph += SIZE_OF_ATTR;
        
        *(int*)ph = (int)schema->dataTypes[i];
        ph += 4;
        
        *(int*)ph = schema->typeLength[i];
        ph += 4;
    }

    SM_FileHandle smFileHandle;
    RC rc;
    rc = createPageFile(name);
    if (rc != RC_OK) return rc;
    rc = openPageFile(name, &smFileHandle);
    if (rc != RC_OK) return rc;
    rc= writeBlock(0, &smFileHandle, data);
    if (rc != RC_OK) return rc;
    return closePageFile(&smFileHandle);
}


//to initialize the buffer pool and loads the schema and metadata in the memory.
extern RC openTable(RM_TableData *rel, char *name) {
    rel->mgmtData = recordManager;
    rel->name = name;

    BM_BufferPool *bm = &recordManager->bufferPool;
    BM_PageHandle *page = &recordManager->pageHandle;

    pinPage(bm, page, 0);
    char *data = page->data;

    recordManager->tupleCount = *(int*)data;
    data += 4;

    recordManager->freePage = *(int*)data;
    data += 4;

    int attCount = *(int*)data;
    data += 4;

    Schema *schema = malloc(sizeof(Schema));
    schema->numAttr = attCount;
    schema->attrNames = malloc(attCount * sizeof(char*));
    schema->dataTypes = malloc(attCount * sizeof(DataType));
    schema->typeLength = malloc(attCount * sizeof(int));
    
    for (int i = 0; i < attCount; i++) {
        schema->attrNames[i] = malloc(SIZE_OF_ATTR);
        strncpy(schema->attrNames[i], data, SIZE_OF_ATTR);
        data =data+ SIZE_OF_ATTR;

        schema->dataTypes[i] = *(int*)data;
        data += 4;

        schema->typeLength[i] = *(int*)data;
        data += 4;
    }

    rel->schema = schema;
    unpinPage(bm, page);
    forcePage(bm, page);
    
    return RC_OK;
}


//close the table by getting the resources used in the buffer pool and shutting it down.
extern RC closeTable(RM_TableData *rel) {
    RecordManager *rm = rel->mgmtData;
    shutdownBufferPool(&rm->bufferPool);
    return RC_OK;
}

//to delete the table having table name
extern RC deleteTable(char *name) {
    RC result=destroyPageFile(name);
    return result;
}


//to return the number of tuples in the table referenced 
extern int getNumTuples(RM_TableData *rel) {
    int result=((RecordManager*)rel->mgmtData)->tupleCount;
    return result;
}


///////////////////////////////////////////////////////
// handling records in a table
//to insert a new record in the table referenced by "rel" and 
//updates the 'record' parameter with the Record ID of he newly inserted record
extern RC insertRecord(RM_TableData *rel, Record *record) {
    RecordManager *rm = rel->mgmtData;
    RID *id = &record->id;
    id->page = rm->freePage;
    
    BM_BufferPool *bm = &rm->bufferPool;
    BM_PageHandle *page = &rm->pageHandle;
    
    pinPage(bm, page, id->page);
    
    id->slot = FreeSlot(page->data, getRecordSize(rel->schema));
    while (id->slot == -1) {
        unpinPage(bm, page);
        id->page++;
        pinPage(bm, page, id->page);
        id->slot = FreeSlot(page->data, getRecordSize(rel->schema));
    }
    
    char *slot = page->data + (id->slot * getRecordSize(rel->schema));
    *slot = '+';
    memcpy(slot + 1, record->data + 1, getRecordSize(rel->schema) - 1);
    
    markDirty(bm, page);
    unpinPage(bm, page);
    
    rm->tupleCount=rm->tupleCount+1;
    pinPage(bm, page, 0);
    
    return RC_OK;
}


//to delete a record having Record ID "id" in the table referenced by "rel"
extern RC deleteRecord(RM_TableData *rel, RID id) {
    RecordManager *rm = rel->mgmtData;
    BM_BufferPool *bm = &rm->bufferPool;
    BM_PageHandle *page = &rm->pageHandle;
    
    pinPage(bm, page, id.page);
    rm->freePage = id.page;
    
    char *data = page->data + (id.slot * getRecordSize(rel->schema));
    *data = '-';
    
    markDirty(bm, page);
    unpinPage(bm, page);
    
    return RC_OK;
}


//to update a record referenced by "record" in the table referenced by "rel"
extern RC updateRecord(RM_TableData *rel, Record *record) {
    RecordManager *rm = rel->mgmtData;
    BM_BufferPool *bm = &rm->bufferPool;
    BM_PageHandle *page = &rm->pageHandle;
    
    pinPage(bm, page, record->id.page);
    
    char *data = page->data + (record->id.slot * getRecordSize(rel->schema));
    *data = '+';
    memcpy(data + 1, record->data + 1, getRecordSize(rel->schema) - 1);
    
    markDirty(bm, page);
    unpinPage(bm, page);
    
    return RC_OK;
}


//to retrieve the record having Record ID "id" in the table referenced by "rel" 
//and the result record is stored in the location referenced by "record"
extern RC getRecord(RM_TableData *rel, RID id, Record *record) {
    RecordManager *rm = rel->mgmtData;
    BM_BufferPool *bm = &rm->bufferPool;
    BM_PageHandle *page = &rm->pageHandle;
    
    pinPage(bm, page, id.page);
    
    char *data = page->data + (id.slot * getRecordSize(rel->schema));
    if (*data != '+') {
        unpinPage(bm, page);
        return RC_RM_NO_TUPLE_WITH_GIVEN_RID;
    }
    
    record->id = id;
    memcpy(record->data + 1, data + 1, getRecordSize(rel->schema) - 1);
    
    unpinPage(bm, page);
    
    return RC_OK;
}


///////////////////////////////////////////////////////////////////////
// scans

//to scan all the records using the condition by entering into the table
extern RC startScan(RM_TableData *rel, RM_ScanHandle *scan, Expr *cond) {
    if (!cond) return RC_SCAN_CONDITION_NOT_FOUND;
    
    RecordManager *scanData = malloc(sizeof(RecordManager));
    scan->mgmtData = scanData;
    scanData->recordID.page = 1;
    scanData->recordID.slot = 0;
    scanData->cond = cond;
    scanData->scanCount = 0;
    
    scan->rel = rel;
    return RC_OK;
}


//to iterate over the records in the table by applying a scan condition and returns the next record which matches.
extern RC next(RM_ScanHandle *scan, Record *record) {
    RecordManager *scanData = scan->mgmtData;
    RecordManager *tableData = scan->rel->mgmtData;
    Schema *schema = scan->rel->schema;
    
    if (!scanData->cond) return RC_SCAN_CONDITION_NOT_FOUND;
    if (scanData->scanCount >= tableData->tupleCount) return RC_RM_NO_MORE_TUPLES;
    
    Value *result = malloc(sizeof(Value));
    while (tableData->tupleCount > scanData->scanCount) {
        scanData->recordID.slot+=1;
            if (scanData->recordID.slot >= PAGE_SIZE / getRecordSize(schema)) {
                scanData->recordID.slot = 0;
                scanData->recordID.page+=1;
      
        } else {
             if (scanData->scanCount != 0) {
            scanData->recordID.page = 1;
            scanData->recordID.slot = 0;
            }
        }
        
        RC rc = getRecord(scan->rel, scanData->recordID, record);
        if (rc == RC_OK) {
            evalExpr(record, schema, scanData->cond, &result);
            if (result->v.boolV) {
                scanData->scanCount++;
                free(result);
                return RC_OK;
            }
        }
        scanData->scanCount++;
    }
    
    free(result);
    return RC_RM_NO_MORE_TUPLES;
}


//to close the scan by resetting the scan manager page, slot and the record count
extern RC closeScan(RM_ScanHandle *scan) {
    RecordManager *scanData = scan->mgmtData;
    free(scanData);
    scan->mgmtData = NULL;
    return RC_OK;
}


////////////////////////////////////////////////////////////////////
// dealing with schemas


//to return the record size of the schema 
extern int getRecordSize(Schema *schema) {
    int size = 1;  // for tombstone
    for (int i = 0; i < schema->numAttr; i++) {
        switch (schema->dataTypes[i]) {
            case DT_INT:
            case DT_FLOAT:
                size += 4;
                break;
            case DT_BOOL:
                size += 1;
                break;
            case DT_STRING:
                size += schema->typeLength[i];
                break;
        }
    }
    return size;
}



//to initialize and return a new Schema structure
extern Schema *createSchema(int numAttr, char **attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys) {
    Schema *schema = malloc(sizeof(Schema));
    schema->numAttr = numAttr;
    schema->attrNames = attrNames;
    schema->dataTypes = dataTypes;
    schema->typeLength = typeLength;
    schema->keySize = keySize;
    schema->keyAttrs = keys;
    return schema;
}


//to remove a schema from memory and to  deallocate all the memory space allocated to the schema.
extern RC freeSchema(Schema *schema) {
    free(schema->attrNames);
    free(schema->dataTypes);
    free(schema->typeLength);
    free(schema->keyAttrs);
    free(schema);
    return RC_OK;
}


/////////////////////////////////////////////////
// dealing with records and attribute values

//to create a new record in the schema referenced by "schema"
extern RC createRecord(Record **record, Schema *schema) {
    *record = malloc(sizeof(Record));
    (*record)->data = malloc(getRecordSize(schema));
    (*record)->id.page = -1;
    (*record)->id.slot = -1;
    (*record)->data[0] = '-';
    return RC_OK;
}



//to remove the record from the memory.
extern RC freeRecord(Record *record) {
    free(record->data);
    free(record);
    return RC_OK;
}



//to retrieve an attribute from the given record in the specified schema in the database
extern RC getAttr(Record *record, Schema *schema, int attrNum, Value **value) {
    *value = malloc(sizeof(Value));
    char *data = record->data + 1;  // skip tombstone
    
    for (int i = 0; i < attrNum; i++) {
        switch (schema->dataTypes[i]) {
            case DT_INT:
            case DT_FLOAT:
                data += sizeof(int);
                break;
            case DT_BOOL:
                data += sizeof(bool);
                break;
            case DT_STRING:
                data += schema->typeLength[i];
                break;
        }
    }
    
    (*value)->dt = schema->dataTypes[attrNum];
    switch (schema->dataTypes[attrNum]) {
        case DT_INT:
            (*value)->v.intV = *(int*)data;
            break;
        case DT_FLOAT:
            (*value)->v.floatV = *(float*)data;
            break;
        case DT_BOOL:
            (*value)->v.boolV = *(bool*)data;
            break;
        case DT_STRING:
            (*value)->v.stringV = malloc(schema->typeLength[attrNum] + 1);
            strncpy((*value)->v.stringV, data, schema->typeLength[attrNum]);
            (*value)->v.stringV[schema->typeLength[attrNum]] = '\0';
            break;
    }
    
    return RC_OK;
}


//to set the attribute value in the record in the specified schema
extern RC setAttr(Record *record, Schema *schema, int attrNum, Value *value) {
    char *data = record->data + 1;  // skip tombstone
    
    for (int i = 0; i < attrNum; i++) {
        switch (schema->dataTypes[i]) {
            case DT_INT:
            case DT_FLOAT:
                data += sizeof(int);
                break;
            case DT_BOOL:
                data += sizeof(bool);
                break;
            case DT_STRING:
                data += schema->typeLength[i];
                break;
        }
    }
    
    switch (schema->dataTypes[attrNum]) {
        case DT_INT:
            *(int*)data = value->v.intV;
            break;
        case DT_FLOAT:
            *(float*)data = value->v.floatV;
            break;
        case DT_BOOL:
            *(bool*)data = value->v.boolV;
            break;
        case DT_STRING:
            strncpy(data, value->v.stringV, schema->typeLength[attrNum]);
            break;
    }
    
    return RC_OK;
}