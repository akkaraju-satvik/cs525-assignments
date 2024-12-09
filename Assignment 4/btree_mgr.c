#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "dberror.h"
#include "btree_mgr.h"
#include "storage_mgr.h"
#include "buffer_mgr.h"
#include "tables.h"

typedef struct NodeData {
    RID rid;
} NodeData;

typedef struct Node {
    void **pointers;
    Value **keys;
    struct Node *parent;
    bool is_leaf;
    int num_keys;
    struct Node *next;
} Node;

typedef struct BTreeManager {
    BM_BufferPool bufferPool;
    BM_PageHandle pageHandler;
    int order;
    int numNodes;
    int numEntries;
    Node *root;
    Node *queue;
    DataType keyType;
} BTreeManager;

typedef struct ScanManager {
    int keyIndex;
    int totalKeys;
    int order;
    Node *node;
} ScanManager;

Node *findLeaf_BTree(Node *root, Value *key);
NodeData *findRecord_BTree(Node *root, Value *key);
void enqueue_BTree(BTreeManager *treeManager, Node *new_node);
Node *dequeue_BTree(BTreeManager *treeManager);
int path_to_root_BTree(Node *root, Node *child);
NodeData *makeRecord_BTree(RID *rid);
Node *insertIntoLeaf_BTree(BTreeManager *treeManager, Node *leaf, Value *key, NodeData *pointer);
Node *createNewTree_BTree(BTreeManager *treeManager, Value *key, NodeData *pointer);
Node *createNode_BTree(BTreeManager *treeManager);
Node *createLeaf_BTree(BTreeManager *treeManager);
Node *insertIntoLeafAfterSplitting_BTree(BTreeManager *treeManager, Node *leaf, Value *key, NodeData *pointer);
Node *insertIntoNode_BTree(BTreeManager *treeManager, Node *parent, int left_index, Value *key, Node *right);
Node *insertIntoNodeAfterSplitting_BTree(BTreeManager *treeManager, Node *parent, int left_index, Value *key, Node *right);
Node *insertIntoParent_BTree(BTreeManager *treeManager, Node *left, Value *key, Node *right);
Node *insertIntoNewRoot_BTree(BTreeManager *treeManager, Node *left, Value *key, Node *right);
int getLeftIndex_BTree(Node *parent, Node *left);
Node *adjustRoot_BTree(Node *root);
Node *mergeNodes_BTree(BTreeManager *treeManager, Node *n, Node *neighbor, int neighbor_index, Value *k_prime);
Node *redistributeNodes_BTree(Node *root, Node *n, Node *neighbor, int neighbor_index, int k_prime_index, Value *k_prime);
Node *deleteEntry_BTree(BTreeManager *treeManager, Node *n, Value *key, void *pointer);
Node *delete_BTree(BTreeManager *treeManager, Value *key);
Node *removeEntryFromNode_BTree(BTreeManager *treeManager, Node *n, Value *key, Node *pointer);
int getNeighborIndex_BTree(Node *n);
bool isLess_BTree(Value *key1, Value *key2);
bool isGreater_BTree(Value *key1, Value *key2);
bool isEqual_BTree(Value *key1, Value *key2);

BTreeManager *treeManager = '\0';

RC initIndexManager(void *mgmtData) {
    initStorageManager();
    return RC_OK;
}

RC shutdownIndexManager() {
    treeManager = '\0';
    return RC_OK;
}

RC createBtree(char *idxId, DataType keyType, int n) {
    int maxNodes = PAGE_SIZE / sizeof(Node);
    if (maxNodes < n) {
        return RC_ORDER_TOO_HIGH_FOR_PAGE;
    }
    
    treeManager = (BTreeManager *) malloc(sizeof(BTreeManager));
     treeManager->root ='\0';
     treeManager->keyType = keyType;
    treeManager->numNodes = 0;
   treeManager->order = n + 2;
    treeManager->queue ='\0';
     treeManager->numEntries = 0;
   
    
    BM_BufferPool *bm = (BM_BufferPool *) malloc(sizeof(BM_BufferPool));
    treeManager->bufferPool = *bm;
    RC result;
    SM_FileHandle fileHandler;
    char data[PAGE_SIZE];
    switch ((result = createPageFile(idxId))) {
    case RC_OK:
        break;
    default:
        return result;
}

switch ((result = openPageFile(idxId, &fileHandler))) {
    case RC_OK:
        break;
    default:
        return result;
}

switch ((result = writeBlock(0, &fileHandler, data))) {
    case RC_OK:
        break;
    default:
        return result;
}

switch ((result = closePageFile(&fileHandler))) {
    case RC_OK:
        break;
    default:
        return result;
}
    return RC_OK;
}

RC openBtree(BTreeHandle **tree, char *idxId) {
  
    *tree = (BTreeHandle *) malloc(sizeof(BTreeHandle));
    if (*tree == '\0') {
        return RC_ERROR; 
    }

    (*tree)->mgmtData = treeManager;

    RC result = initBufferPool(&treeManager->bufferPool, idxId, 1000, RS_FIFO, NULL);
    if (result != RC_OK) {
        return result;
        free(*tree); 
    }
    result=RC_OK;
    return result;
}

RC closeBtree(BTreeHandle *tree) {
    if(tree == NULL){
        return RC_ERROR;
    }
    
    BTreeManager *treeManager = (BTreeManager*) tree->mgmtData;
    if(tree!=NULL){
        markDirty(&(treeManager->bufferPool), &(treeManager->pageHandler));
        shutdownBufferPool(&(treeManager->bufferPool));
        free(treeManager);
        
    }
    free(tree);
    return RC_OK;
}

RC deleteBtree(char *idxId) {
    RC result = destroyPageFile(idxId);
    if (result != RC_OK)
        return result;
    return RC_OK;
}


RC insertKey(BTreeHandle *tree, Value *key, RID rid) {
      Node *leaf;
    NodeData *pointer;
    BTreeManager *treeManager = (BTreeManager *) tree->mgmtData;
    RC result ;
    int bTreeOrder = treeManager->order;
   
    if (findRecord_BTree(treeManager->root, key)!= '\0') {
        return RC_IM_KEY_ALREADY_EXISTS;
    }

    pointer = makeRecord_BTree(&rid);
    
    if (treeManager->root == '\0') {
        treeManager->root = createNewTree_BTree(treeManager, key, pointer);
        result=RC_OK;
        return result;
    }

    leaf = findLeaf_BTree(treeManager->root, key);
    int leaf_num_keys = leaf->num_keys;
    if ( leaf_num_keys < bTreeOrder - 1) {
        leaf = insertIntoLeaf_BTree(treeManager, leaf, key, pointer);
    }
   
    else {
        treeManager->root = insertIntoLeafAfterSplitting_BTree(treeManager, leaf, key, pointer);
    }
    result=RC_OK;
    return result;
}

Node * createLeaf_BTree(BTreeManager * treeManager) {
	Node * leaf = createNode_BTree(treeManager);
	leaf->is_leaf = true;
	return leaf;
}

RC findKey(BTreeHandle *tree, Value *key, RID *result) {
    NodeData *r = findRecord_BTree(treeManager->root, key);
    BTreeManager *treeManager = (BTreeManager *) tree->mgmtData;
    
    if (r == '\0') {
        return RC_IM_KEY_NOT_FOUND;
    } else {
        *result = r->rid;
        return RC_OK;
    }
}

Node * insertIntoNewRoot_BTree(BTreeManager * treeManager, Node * left, Value * key, Node * right) {
	Node * root = createNode_BTree(treeManager);
   root->pointers[1] = right;
  	root->keys[0] = key;
  root->pointers[0] = left;
   root->num_keys=root->num_keys+1;

  left->parent = root;
	right->parent = root;
	root->parent = NULL;
	return root;
}

Node * insertIntoNode_BTree(BTreeManager * treeManager, Node * parent, int left_index, Value * key, Node * right) {
    int i = parent->num_keys;
    while (i > left_index) {
        parent->keys[i] = parent->keys[i - 1];
        parent->pointers[i + 1] = parent->pointers[i];
        i=i-1;
    }

    parent->pointers[left_index + 1] = right;
    parent->num_keys=parent->num_keys+1;
    parent->keys[left_index] = key;
    

    return treeManager->root;
}

Node * insertIntoNodeAfterSplitting_BTree(BTreeManager * treeManager, Node * old_node, int left_index, Value * key, Node * right) {

     Value ** temp_keys;
    Node * new_node, *child;
    Node ** temp_pointers;
    int i=0, j, split;
    Value * k_prime;
  
    int bTreeOrder = treeManager->order;

    temp_pointers = malloc((bTreeOrder + 1) * sizeof(Node *));
    if (temp_pointers == '\0') {
        exit(RC_INSERT_ERROR);
    }
    temp_keys = malloc(bTreeOrder * sizeof(Value *));
    if (temp_keys == '\0') {
        exit(RC_INSERT_ERROR);
    }

    i = 0; j = 0;
    while (i < old_node->num_keys + 1) {
        if (j == left_index + 1)
            j=j+1;
        temp_pointers[j] = old_node->pointers[i];
        j=j+1;
        i=i+1;
        
    }

    i = 0; j = 0;
    while (i < old_node->num_keys) {
        if (j == left_index)
            j=j+1;
        temp_keys[j] = old_node->keys[i];
         j=j+1;
        i=i+1;
       
    }
        temp_keys[left_index] = key;
    temp_pointers[left_index + 1] = right;


    if ((bTreeOrder - 1) % 2 != 0)
      split = (bTreeOrder - 1) / 2 + 1;
        
    else
        split = (bTreeOrder - 1) / 2;

    new_node = createNode_BTree(treeManager);
    old_node->num_keys = 0;
    i = 0;
    while (i < split - 1) {
        old_node->keys[i] = temp_keys[i];
        old_node->pointers[i] = temp_pointers[i];
        old_node->num_keys=old_node->num_keys+1;
        i=i+1;
    }
     k_prime = temp_keys[split - 1];
    old_node->pointers[i] = temp_pointers[i];
   

    i=i+1; j = 0;
    while (i < bTreeOrder) {
        new_node->pointers[j] = temp_pointers[i];
        new_node->num_keys=new_node->num_keys+1;
         new_node->keys[j] = temp_keys[i];
        j=j+1;
        i=i+1;
        
    }
    new_node->pointers[j] = temp_pointers[i];
    free(temp_pointers);
    free(temp_keys);
    new_node->parent = old_node->parent;
    
    i = 0;
    while (i <= new_node->num_keys) {
        child = new_node->pointers[i];
        child->parent = new_node;
        i=i+1;
    }

    treeManager->numEntries++;
    return insertIntoParent_BTree(treeManager, old_node, k_prime, new_node);
}

Node * removeEntryFromNode_BTree(BTreeManager * treeManager, Node * n, Value * key, Node * pointer)  {
    int i=0, num_pointers;
    
   int bTreeOrder = treeManager->order;
   
    for (; !isEqual_BTree(n->keys[i], key); i++) {}
    i=i+1;
    while (i < n->num_keys) {
        n->keys[i - 1] = n->keys[i];
        i=i+1;
    }
    num_pointers = n->is_leaf ? n->num_keys : n->num_keys + 1;
    i = 0;
    for (; n->pointers[i] != pointer; i++) {}
    i=i+1;
    while (i < num_pointers) {
        n->pointers[i - 1] = n->pointers[i];
        i=i+1;
    }

    n->num_keys=n->num_keys-1;
    treeManager->numEntries= treeManager->numEntries-1;
    if (!n->is_leaf) {
       i = n->num_keys + 1;
        while (i < bTreeOrder) {
            n->pointers[i] = '\0';
            i=i+1;
        }
       
    } else {
       i = n->num_keys;
        while (i < bTreeOrder - 1) {
            n->pointers[i] = '\0';
            i=i+1;
        }
       
    }

    return n;
}

Node * adjustRoot_BTree(Node * root) {

	Node * new_root;
	if (root->num_keys > 0)
		return root;

	if (!root->is_leaf) {
    new_root->parent = '\0';
		new_root = root->pointers[0];
		
	} else {
		new_root = '\0';
	}
    free(root->pointers);
	free(root->keys);
	free(root);

	return new_root;
}

Node * createNode_BTree(BTreeManager * treeManager) {
    int bTreeOrder = treeManager->order;
	treeManager->numNodes=treeManager->numNodes+1;

	Node * new_node = malloc(sizeof(Node));
	if (new_node == '\0') {
		exit(RC_INSERT_ERROR);
	}


	new_node->keys = malloc((bTreeOrder - 1) * sizeof(Value *));
	if (new_node->keys == '\0') {
		exit(RC_INSERT_ERROR);
	}

	new_node->pointers = malloc(bTreeOrder * sizeof(void *));
	if (new_node->pointers == '\0') {
		exit(RC_INSERT_ERROR);
	}
    
    new_node->next = '\0';
    new_node->parent = '\0';
	new_node->num_keys = 0;
	new_node->is_leaf = false;
	return new_node;
}

int getLeftIndex_BTree(Node * parent, Node * left) {
    int left_index = 0;
    for (; left_index <= parent->num_keys && parent->pointers[left_index] != left; left_index++) {}
    return left_index;
}

int getNeighborIndex_BTree(Node * n) {
    int i = 0;
    
    while (i <= n->parent->num_keys) {
        if (n->parent->pointers[i] == n)
            return i - 1;
        i=i+1;
    }
    exit(RC_ERROR);
}

Node * deleteEntry_BTree(BTreeManager * treeManager, Node * n, Value * key, void * pointer) {
	int min_keys;
	Node * neighbor;
    int bTreeOrder = treeManager->order;
    int neighbor_index;
	int k_prime_index;
    int capacity;
    Value * k_prime;


	n = removeEntryFromNode_BTree(treeManager, n, key, pointer);
    Node * r;
	if (n == treeManager->root){
		 r=adjustRoot_BTree(treeManager->root);
         return r;
    }
	if (!n->is_leaf) {
    if ((bTreeOrder) % 2 == 0)
			min_keys = (bTreeOrder) / 2;
		else
			min_keys = (bTreeOrder) / 2 + 1;
		min_keys=min_keys-1;
	
	} else {
			if ((bTreeOrder - 1) % 2 == 0)
			min_keys = (bTreeOrder - 1) / 2;
		else
			min_keys = (bTreeOrder - 1) / 2 + 1;
	}
	if (n->num_keys >= min_keys)
		return treeManager->root;
	neighbor_index = getNeighborIndex_BTree(n);
    if(neighbor_index == -1 )
    {
        k_prime_index = 0;
   }
    else
    {
       k_prime_index = neighbor_index;
    }
	k_prime = n->parent->keys[k_prime_index];
    if (neighbor_index == -1)
    {
        neighbor=n->parent->pointers[1];
    }
    else
    {
        neighbor=n->parent->pointers[neighbor_index];
    }
	
	if(n->is_leaf)
    {
        capacity=bTreeOrder ;
    }
    else{
        capacity=bTreeOrder - 1;
    }

	if (neighbor->num_keys + n->num_keys >= capacity)

  	return redistributeNodes_BTree(treeManager->root, n, neighbor, neighbor_index, k_prime_index, k_prime);
	else
		return mergeNodes_BTree(treeManager, n, neighbor, neighbor_index, k_prime);
}

Node * findLeaf_BTree(Node * root, Value * key) {
    Node * c = root;
    if (c == '\0') {
        return c;
    }

    for (; !c->is_leaf; ) {
        int i = 0;
        for (i = 0; i < c->num_keys; i++) {
            if (!(isGreater_BTree(key, c->keys[i]) || isEqual_BTree(key, c->keys[i]))) {
                break;
            }
        }

        c = (Node *) c->pointers[i];
    }

    return c;
}

NodeData * findRecord_BTree(Node * root, Value *key) {
    Node * c = findLeaf_BTree(root, key);
    int i = 0;
  
    if (c == '\0')
        return NULL;
    while (i < c->num_keys) {
        if (isEqual_BTree(c->keys[i], key))
            break;
        i=i+1;
    }

    if (i != c->num_keys)
       return (NodeData *) c->pointers[i];
    else
    return NULL;
        
}

Node * insertIntoParent_BTree(BTreeManager * treeManager, Node * left, Value * key, Node * right) {
	int left_index;
    int bTreeOrder = treeManager->order;
	Node * parent = left->parent;
    

	if (parent == '\0')
		return insertIntoNewRoot_BTree(treeManager, left, key, right);

	left_index = getLeftIndex_BTree(parent, left);
	if (parent->num_keys >= bTreeOrder - 1) {
    	return insertIntoNodeAfterSplitting_BTree(treeManager, parent, left_index, key, right);
	
	}
	return insertIntoNode_BTree(treeManager, parent, left_index, key, right);

}

RC getNumEntries(BTreeHandle *tree, int *result) {
    BTreeManager *treeManager = (BTreeManager *) tree->mgmtData;
    *result = treeManager->numEntries;
    return RC_OK;
}

Node * delete(BTreeManager * treeManager, Value * key) {
    NodeData * key_leaf = findLeaf_BTree(treeManager->root, key);
	Node * record = findRecord_BTree(treeManager->root, key);

	if (record != NULL && key_leaf != NULL  ) {
		treeManager->root = deleteEntry_BTree(treeManager, key_leaf, key, record);
		free(record);
	}
	return treeManager->root;
}

RC getNumNodes(BTreeHandle *tree, int *result) {
    BTreeManager *treeManager ;
    treeManager = (BTreeManager *) tree->mgmtData;
    int result_nodes = treeManager->numNodes;
    *result = result_nodes;
    return RC_OK;
}

RC getKeyType(BTreeHandle *tree, DataType *result) {
    BTreeManager *treeManager ;
    treeManager = (BTreeManager *) tree->mgmtData;
    *result = treeManager->keyType;
    return RC_OK;
}

RC openTreeScan(BTreeHandle *tree, BT_ScanHandle **handle) {
    BTreeManager *treeManager;
    treeManager = (BTreeManager *) tree->mgmtData;
    ScanManager *scanmeta ;
    scanmeta = malloc(sizeof(ScanManager));
    *handle = malloc(sizeof(BT_ScanHandle));
    Node *node = treeManager->root;
    if (treeManager->root !='\0') 
    {
        for (; !node->is_leaf; node = node->pointers[0]) {}
        scanmeta->totalKeys = node->num_keys;
        scanmeta->keyIndex = 0;
        scanmeta->order = treeManager->order;
        scanmeta->node = node;
        (*handle)->mgmtData = scanmeta;
    }

     else {
         return RC_NO_RECORDS_TO_SCAN; 
    }
    return RC_OK;
}

RC deleteKey(BTreeHandle *tree, Value *key) {
    BTreeManager *treeManager; 
    treeManager = (BTreeManager *) tree->mgmtData;
    treeManager->root = delete(treeManager, key);
    return RC_OK;
}

RC nextEntry(BT_ScanHandle *handle, RID *result) {
    ScanManager *scanData = (ScanManager *)handle->mgmtData;
    Node *currentNode = scanData->node;
    
    if (currentNode == NULL) {
        return RC_IM_NO_MORE_ENTRIES;
    }
    
    if (scanData->keyIndex < scanData->totalKeys) {
        *result = ((NodeData *)currentNode->pointers[scanData->keyIndex])->rid;
        scanData->keyIndex++;
        return RC_OK;
    }
    
    if (currentNode->pointers[scanData->order - 1] != NULL) {
        currentNode = currentNode->pointers[scanData->order - 1];
        scanData->node = currentNode;
        scanData->keyIndex = 1;
        scanData->totalKeys = currentNode->num_keys;
        *result = ((NodeData *)currentNode->pointers[0])->rid;
        return RC_OK;
    }
    
    return RC_IM_NO_MORE_ENTRIES;
}

RC closeTreeScan(BT_ScanHandle *handle) {
    if (handle == NULL) {
        return RC_ERROR;  
    }
    if (!handle->mgmtData) {
        free(handle->mgmtData);
        handle->mgmtData = '\0'; 
    }
    free(handle);

    return RC_OK;
}

char *printTree(BTreeHandle *tree) {
    BTreeManager *treeManager = (BTreeManager *) tree->mgmtData;
  
    Node *n = '\0';
    int i = 0;
    int rank = 0;
    int new_rank = 0;
    
    if (treeManager->root == '\0') {
        return NULL;
    }
    
    treeManager->queue = '\0';
    enqueue_BTree(treeManager, treeManager->root);
    while (treeManager->queue != '\0') {
        n = dequeue_BTree(treeManager);
        if (n->parent != '\0' && n == n->parent->pointers[0]) {
            new_rank = path_to_root_BTree(treeManager->root, n);
            if (new_rank != rank) {
                rank = new_rank;
            }
        }
        
        for (i = 0; i < n->num_keys; i++) {
            if (treeManager->keyType == DT_INT) {
                printf("%d ", (*n->keys[i]).v.intV);
            } else if (treeManager->keyType == DT_FLOAT) {
                printf("%.02f ", (*n->keys[i]).v.floatV);
            } else if (treeManager->keyType == DT_STRING) {
                printf("%s ", (*n->keys[i]).v.stringV);
            } else if (treeManager->keyType == DT_BOOL) {
                printf("%d ", (*n->keys[i]).v.boolV);
            }
            printf("(%d - %d) ", ((NodeData *) n->pointers[i])->rid.page, ((NodeData *) n->pointers[i])->rid.slot);
        }
        
        if (!n->is_leaf){
            for (i = 0; i <= n->num_keys; i++)
            {
                if(n->pointers!=NULL){
                    enqueue_BTree(treeManager, n->pointers[i]);
                }
                
            }
                
        }
            
    }
    return NULL;
}


NodeData *makeRecord_BTree(RID *rid) {
    if (rid == NULL) {
        return NULL;
    }

    NodeData *record = malloc(sizeof(NodeData));
    if (record == NULL) {
        return NULL;
    }

    record->rid = *rid;
    return record;
}

Node *createNewTree_BTree(BTreeManager *treeManager, Value *key, NodeData *pointer) {
    Node *root = createLeaf_BTree(treeManager);
    int bTreeOrder = treeManager->order;
    root->pointers[0] = pointer;
    root->pointers[bTreeOrder - 1] = NULL;
    root->parent = NULL;
    root->keys[0] = key;
    root->num_keys=root->num_keys+1;
    treeManager->numEntries=treeManager->numEntries+1;
    return root;
}

Node *insertIntoLeaf_BTree(BTreeManager *treeManager, Node *leaf, Value *key, NodeData *pointer) {
    int i, insertion_point;
     insertion_point = 0;
    treeManager->numEntries=treeManager->numEntries+1;
   
    while (insertion_point < leaf->num_keys && isLess_BTree(leaf->keys[insertion_point], key))
        insertion_point=insertion_point+1;
    for (i = leaf->num_keys; i > insertion_point; i--) {
         
        leaf->pointers[i] = leaf->pointers[i - 1];
        leaf->keys[i] = leaf->keys[i - 1];
         
    }

      leaf->pointers[insertion_point] = pointer;
      leaf->num_keys=leaf->num_keys+1;
    leaf->keys[insertion_point] = key;

    
    return leaf;
}

Node *insertIntoLeafAfterSplitting_BTree(BTreeManager *treeManager, Node *leaf, Value *key, NodeData *pointer) {
    Node *new_leaf;
    Value **temp_keys;
    void **temp_pointers;
    int insertion_index, split, i, j;
    Value *new_key;

    new_leaf = createLeaf_BTree(treeManager);
    int bTreeOrder = treeManager->order;

    temp_keys = malloc(bTreeOrder * sizeof(Value *));
    if (temp_keys == '\0') {
        exit(RC_INSERT_ERROR);
    }

    temp_pointers = malloc(bTreeOrder * sizeof(void *));
    if (temp_pointers == '\0') {
        exit(RC_INSERT_ERROR);
    }

    insertion_index = 0;
    for (; insertion_index < bTreeOrder - 1 && isLess_BTree(leaf->keys[insertion_index], key); insertion_index++) {}

    i = 0;
    j = 0;
    while (i < leaf->num_keys) {
        if (j == insertion_index) 
        j=j+1;
        temp_pointers[j] = leaf->pointers[i];
        temp_keys[j] = leaf->keys[i];
         j=j+1;
        i=i+1;
       
    }
    
    temp_pointers[insertion_index] = pointer;
    temp_keys[insertion_index] = key;
    leaf->num_keys = 0;

    if ((bTreeOrder - 1) % 2 != 0)
    split = (bTreeOrder - 1) / 2 + 1;
      
    else
      split = (bTreeOrder - 1) / 2;
        

    i = 0;
    while (i < split) {
        leaf->pointers[i] = temp_pointers[i];
        leaf->keys[i] = temp_keys[i];
        leaf->num_keys=leaf->num_keys+1;
        i=i+1;
    }

    i = split;
    j = 0;
    while (i < bTreeOrder) {
        new_leaf->pointers[j] = temp_pointers[i];
        new_leaf->keys[j] = temp_keys[i];
        new_leaf->num_keys=new_leaf->num_keys+1;
        i=i+1;
        j=j+1;
    }

    free(temp_pointers);
    free(temp_keys);

    new_leaf->pointers[bTreeOrder - 1] = leaf->pointers[bTreeOrder - 1];
    leaf->pointers[bTreeOrder - 1] = new_leaf;

    i = leaf->num_keys;
    while (i < bTreeOrder - 1) {
        leaf->pointers[i] = NULL;
        i=i+1;
    }

    i = new_leaf->num_keys;
    while (i < bTreeOrder - 1) {
        new_leaf->pointers[i] = NULL;
        i=i+1;
    }

    new_leaf->parent = leaf->parent;
    new_key = new_leaf->keys[0];

    treeManager->numEntries++;

    return insertIntoParent_BTree(treeManager, leaf, new_key, new_leaf);
}

Node *mergeNodes_BTree(BTreeManager *treeManager, Node *n, Node *neighbor, int neighbor_index, Value *k_prime) {
    int i, j, neighbor_insertion_index, n_end;
    int bTreeOrder = treeManager->order;
    Node *tmp;

    if (neighbor_index == -1) {
        tmp = n;
        n = neighbor;
        neighbor = tmp;
    }

    neighbor_insertion_index = neighbor->num_keys;

    if (!n->is_leaf) {
        neighbor->num_keys++;
        neighbor->keys[neighbor_insertion_index] = k_prime;
        

        n_end = n->num_keys;
        i = neighbor_insertion_index + 1;
        j = 0;
        while (j < n_end) {
            neighbor->keys[i] = n->keys[j];
            neighbor->pointers[i] = n->pointers[j];
            neighbor->num_keys=neighbor->num_keys+1;
            n->num_keys=n->num_keys+1;
            j=j+1;
            i=i+1;
            
        }
        neighbor->pointers[i] = n->pointers[j];

        i = 0;
        while (i < neighbor->num_keys + 1) {
            tmp = (Node *)neighbor->pointers[i];
            tmp->parent = neighbor;
            i++;
        }
    } else {
        i = neighbor_insertion_index;
        j = 0;
        while (j < n->num_keys) {
            neighbor->pointers[i] = n->pointers[j];
            neighbor->keys[i] = n->keys[j];
            neighbor->num_keys++;
             j++;
            i++;
           
        }
        neighbor->pointers[bTreeOrder - 1] = n->pointers[bTreeOrder - 1];
    }

    treeManager->root = deleteEntry_BTree(treeManager, n->parent, k_prime, n);
    free(n->keys);
    free(n->pointers);
    free(n);
    return treeManager->root;
}

Node *redistributeNodes_BTree(Node *root, Node *n, Node *neighbor, int neighbor_index, int k_prime_index, Value *k_prime) {
    int i;
    Node *tmp;

    if (neighbor_index != -1) {
        if (!n->is_leaf)
            n->pointers[n->num_keys + 1] = n->pointers[n->num_keys];
        for (i = n->num_keys; i > 0; i--) {
             n->pointers[i] = n->pointers[i - 1];
            n->keys[i] = n->keys[i - 1];
           
        }
        if (!n->is_leaf) {
            n->pointers[0] = neighbor->pointers[neighbor->num_keys];
            tmp = (Node *)n->pointers[0];
             neighbor->pointers[neighbor->num_keys] = NULL;
            tmp->parent = n;
            n->keys[0] = k_prime;
            n->parent->keys[k_prime_index] = neighbor->keys[neighbor->num_keys - 1];
        } else {
            n->pointers[0] = neighbor->pointers[neighbor->num_keys - 1];
            n->keys[0] = neighbor->keys[neighbor->num_keys - 1];
            neighbor->pointers[neighbor->num_keys - 1] = NULL;
            n->parent->keys[k_prime_index] = n->keys[0];
        }
    } else {
        if (n->is_leaf) {
            n->keys[n->num_keys] = neighbor->keys[0];
            n->parent->keys[k_prime_index] = neighbor->keys[1];
            n->pointers[n->num_keys] = neighbor->pointers[0];
        
        } else {
            n->keys[n->num_keys] = k_prime;
            n->pointers[n->num_keys + 1] = neighbor->pointers[0];
            tmp = (Node *)n->pointers[n->num_keys + 1];
            tmp->parent = n;
            n->parent->keys[k_prime_index] = neighbor->keys[0];
        }
        for (i = 0; i < neighbor->num_keys - 1; i++) {
            neighbor->pointers[i] = neighbor->pointers[i + 1];
            neighbor->keys[i] = neighbor->keys[i + 1];
        
        }
        if (!n->is_leaf)
            neighbor->pointers[i] = neighbor->pointers[i + 1];
    }

    n->num_keys=n->num_keys+1;
    neighbor->num_keys=neighbor->num_keys-1;

    return root;
}

void enqueue_BTree(BTreeManager *treeManager, Node *new_node) {
    Node *c;
    if (treeManager->queue == NULL) {
        treeManager->queue = new_node;
        treeManager->queue->next = NULL;
    } else {
        c = treeManager->queue;
        while (c->next != NULL) {
            c = c->next;
        }
        c->next = new_node;
        new_node->next = NULL;
    }
}

Node *dequeue_BTree(BTreeManager *treeManager) {
    Node *n = treeManager->queue;
    treeManager->queue = treeManager->queue->next;
    n->next = NULL;
    return n;
}

int path_to_root_BTree(Node *root, Node *child) {
    int length = 0;
    Node *c = child;
    for (; c != root; c = c->parent) {
        length=length+1;
    }
    return length;
}

bool isLess_BTree(Value *key1, Value *key2) {
    if (key1->dt == DT_INT) {
        return (key1->v.intV < key2->v.intV);
    } else if (key1->dt == DT_FLOAT) {
        return (key1->v.floatV < key2->v.floatV);
    } else if (key1->dt == DT_STRING) {
        return (strcmp(key1->v.stringV, key2->v.stringV) < 0);
    } else if (key1->dt == DT_BOOL) {
        return false;
    } else {
        return false;
    }
}

bool isGreater_BTree(Value *key1, Value *key2) {
    if (key1->dt == DT_INT) {
        return (key1->v.intV > key2->v.intV);
    } else if (key1->dt == DT_FLOAT) {
        return (key1->v.floatV > key2->v.floatV);
    } else if (key1->dt == DT_STRING) {
        return (strcmp(key1->v.stringV, key2->v.stringV) > 0);
    } else if (key1->dt == DT_BOOL) {
        return false;
    } else {
        return false;
    }
}

bool isEqual_BTree(Value *key1, Value *key2) {
    if (key1->dt == DT_INT) {
        return (key1->v.intV == key2->v.intV);
    } else if (key1->dt == DT_FLOAT) {
        return (key1->v.floatV == key2->v.floatV);
    } else if (key1->dt == DT_STRING) {
        return (strcmp(key1->v.stringV, key2->v.stringV) == 0);
    } else if (key1->dt == DT_BOOL) {
        return (key1->v.boolV == key2->v.boolV);
    } else {
        return false;
    }
}
