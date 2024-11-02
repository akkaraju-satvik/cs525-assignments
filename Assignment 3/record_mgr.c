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

void strRepInt(int j, int val, char *intStr) {
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

// HANDLING RECORDS IN TABLE

// SCANS

extern RC next(RM_ScanHandle *scan, Record *record) {
    int blockfactor = PAGE_SIZE / recordManager->sizeOfRecord;
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
                if (curSlotScan == (blockfactor - 1)) {
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
        if (curSlotScan == (blockfactor - 1)) {
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
