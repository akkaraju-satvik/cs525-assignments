#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<math.h>

#include "storage_mgr.h"

FILE *fileHandlerPointer;
FILE *nullValueCheck = NULL;

//Method to make the pointer values as null.
void makePointerNull()
{
    fileHandlerPointer = nullValueCheck; 
}

void initStorageManager (void) {
	do{
		makePointerNull(); //Initialize the file pointer to NULL value
	}while(0);
}

int seekFilePointer(FILE *fileHandlerPointer, long int n, int position){
    return fseek(fileHandlerPointer, n, position);
}

void fcloseFilePointer(FILE *fileHandlerPointer){
    fclose(fileHandlerPointer);
}


RC createPageFile(char* fileName){

	char *record = fileName;
    fileHandlerPointer = fopen(record, "w+"); 
	if(fileHandlerPointer == nullValueCheck)
	{
		return RC_FILE_NOT_FOUND;
	}
    else
	{
		SM_PageHandle emptyPageHandler = (SM_PageHandle) calloc(PAGE_SIZE,sizeof(char));
		int writeFileStatus = fwrite(emptyPageHandler, sizeof(char), PAGE_SIZE, fileHandlerPointer);
		if(writeFileStatus == 0)
		{
			return RC_WRITE_FAILED;
		}
        return RC_OK;
        }
}


RC openPageFile (char *fileName, SM_FileHandle *fHandle){
    char *record = fileName;
    fileHandlerPointer = fopen(record, "r"); 
    if(fileHandlerPointer==NULL){
        return RC_FILE_NOT_FOUND;
    }
    fHandle->fileName = record;
    fHandle->curPagePos = 0;
    seekFilePointer(fileHandlerPointer,0, SEEK_END);
    long fileSize = ftell(fileHandlerPointer); // to get the file size
    fHandle->totalNumPages = fileSize /PAGE_SIZE;
    fcloseFilePointer(fileHandlerPointer);
    return RC_OK;


}


RC closePageFile (SM_FileHandle *fHandle){
    
    FILE *fileHandlerPointer = fopen(fHandle->fileName, "rb+");
    if(fileHandlerPointer!=NULL){
        int position = seekFilePointer(fileHandlerPointer,0,SEEK_SET);
        if(position==0){
            fcloseFilePointer(fileHandlerPointer);
            fileHandlerPointer = NULL;
            return RC_OK;
        }
    }
    return RC_FILE_NOT_FOUND;

}

RC destroyPageFile (char *fileName){
    FILE *fileHandlerPointer = fopen(fileName, "rb+");
    if(fileHandlerPointer!=NULL){
        fcloseFilePointer(fileHandlerPointer);
        int mode = remove(fileName);
        if(mode==0){
            return RC_OK;
        }
    }
    
    return RC_FILE_NOT_FOUND;
}

RC readBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage){
    FILE *fileHandlerPointer = fopen(fHandle->fileName,"r");
    if(pageNum <= 0 || pageNum > fHandle->totalNumPages){
        return RC_READ_NON_EXISTING_PAGE;
    }
    if(fileHandlerPointer!=NULL){
        int temp = seekFilePointer(fileHandlerPointer, (pageNum*PAGE_SIZE), SEEK_SET);
        if(temp==0){
            fread(memPage, PAGE_SIZE, sizeof(char), fileHandlerPointer);
            fHandle->curPagePos = ftell(fileHandlerPointer);
            fcloseFilePointer(fileHandlerPointer);
            return RC_OK;
        }else{
            return RC_READ_NON_EXISTING_PAGE;
        }
    }
    fcloseFilePointer(fileHandlerPointer);
    return RC_FILE_NOT_FOUND;
}

int getBlockPos (SM_FileHandle *fHandle){
    FILE *fileHandlerPointer = fopen(fHandle->fileName,"r");
    if(fileHandlerPointer!=NULL){
        int currentPage = fHandle->curPagePos;
        return currentPage;
    }
    return 0;
}

RC readFirstBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return readBlock(0, fHandle, memPage);
}

RC readLastBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return readBlock(fHandle->totalNumPages-1, fHandle, memPage);
}

RC readPreviousBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    int readPreviousBlock = getBlockPos(fHandle)-1;
    return readBlock(readPreviousBlock, fHandle, memPage);
}

RC readCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    int readCurrentBlock = fHandle->curPagePos/PAGE_SIZE;
    return readBlock(readCurrentBlock,fHandle, memPage);
}

RC readNextBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    int readNextBlock = getBlockPos(fHandle) + 1;
    return readBlock(readNextBlock, fHandle, memPage);
}
