#include "record_mgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer_mgr.h"
#include "storage_mgr.h"

typedef struct RecordManager {
    BM_PageHandle *pageHandle;
    BM_BufferPool *bufferPool;
    int sizeOfRecord;
    int numberOfRecords;
    int keySize;
    RID freeSlot;
} RecordManager;

typedef struct ScanManager {
    RID recID;
    Expr *cond;
    int count;  // no of records scaned
    RM_TableData *tbl_data;
    BM_PageHandle pageHandle;
    BM_BufferPool bufferPool;
} ScanManager;

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

extern RC initRecordManager(void *mgmtData) {
    recordManager = (RecordManager *)malloc(sizeof(RecordManager));
    recordManager->bufferPool = (BM_BufferPool *)malloc(sizeof(BM_BufferPool));
    recordManager->pageHandle = (BM_PageHandle *)malloc(sizeof(BM_PageHandle));
    recordManager->pageHandle->data =
        (char *)malloc(sizeof(char) * (PAGE_SIZE));
    return RC_OK;
}

extern RC shutdownRecordManager() {
    shutdownBufferPool(recordManager->bufferPool);
    free(recordManager->bufferPool);
    free(recordManager->pageHandle);
    return RC_OK;
}

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
extern RC openTable(RM_TableData *rel, char *name) {
    initBufferPool(recordManager->bufferPool, name, 3, RS_FIFO, NULL);
    rel->schema = readSchema(name);
    recordManager->numberOfRecords = metadata->numberOfAttributes;
    recordManager->freeSlot = metadata->freeSlot;
    rel->name = name;
    rel->mgmtData = recordManager;
    return RC_OK;
}

extern RC closeTable(RM_TableData *rel) {
    RecordManager *rManager = rel->mgmtData;
    shutdownBufferPool(rManager->bufferPool);
    return RC_OK;
}

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

extern int getNumTuples(RM_TableData *rel) {
    return recordManager->numberOfRecords;
}

// HANDLING RECORDS IN TABLE

// SCANS
extern RC startScan(RM_TableData *rel, RM_ScanHandle *scan, Expr *cond) {
    BM_BufferPool *bm = recordManager->bufferPool;

    scan->rel = rel;
    scanManager.cond = cond;
    scanManager.recID.page = 1;
    scanManager.recID.slot = 0;
    scanManager.count = 0;

    scan->mgmtData = &scanManager;

    return RC_OK;

    return RC_OK;
}
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

extern RC closeScan(RM_ScanHandle *scan) {
    scanManager.recID.page = 1;
    scanManager.recID.slot = 0;
    scanManager.count = 0;
    return RC_OK;
}

// DEALING WITH SCHEMAS

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

extern RC freeSchema(Schema *schema) {
    free(schema);
    return RC_OK;
}

extern RC freeRecord(Record *record) {
    free(record);
    return RC_OK;
}
