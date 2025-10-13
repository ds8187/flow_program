#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include<stdbool.h>


typedef struct {
    char *name;
    char *command;
} Node;

typedef struct {
    char *name;
    char *from;
    char *to;
} Pipe;

typedef struct {
    char *name;
    int partCount;
    int partSize; 
    int partCapacity;
    char **parts;
} Concat;

typedef struct {
    char *name;
    char *from;
} Stderr;

typedef struct{
    Node** list;
    int size;
    int capacity; 
} NodeList;

typedef struct{
    Pipe** list;
    int size;
    int capacity; 
} PipeList;

typedef struct{
    Concat** list;
    int size;
    int capacity; 
} ConcatList;

typedef struct{
    Stderr** list;
    int size;
    int capacity; 
} StderrList;

typedef struct{
    NodeList* nodeList;
    PipeList* pipeList;
    ConcatList* concatList;
    StderrList* stderrList;
} ListContainer;

typedef enum {NODE_TYPE, PIPE_TYPE, CONCAT_TYPE, STDERR_TYPE} NodeType;

typedef union {
    Node* node;
    Pipe* pipe;
    Concat* concat;
    Stderr* stderr;
} NodeData;

typedef struct TreeNode {
    NodeType type;
    NodeData data;
    struct TreeNode* left;
    struct TreeNode* right;
} TreeNode;

typedef struct Tree{
    TreeNode* root;
    int size;
} Tree;

/*
typedef struct {
    char *name;
    int *fileName;
} fileDef;
 */


void execute();
void parseFile(const char *filename, ListContainer* listContainer);
void createNode(ListContainer* listContainer);
void createPipe(ListContainer* listContainer);
void createConcat(ListContainer* listContainer);
void saveNodeName(ListContainer* listContainer, char* lineBuffer);
void saveCmd(ListContainer* listContainer, char* lineBuffer);
void saveConcatAttr(char** concatAttr, char* lineBuffer, int offset);
void saveParts(ConcatList* concatList, char* startOfWord);
void savePipeAttr(char** pipeAttr, char* lineBuffer, int offset);
bool isInRefList(char** refList, int size, char* name);
void createExecutionTree(ListContainer* listContainer, Tree* tree, char* rootName);
char* findRootNode(ListContainer* listContainer);
char* getNodeType(ListContainer* listContainer, char* name);
TreeNode* buildTreeNode(ListContainer* listContainer, char* name);
Node* findNodeByName(NodeList* list, const char* name);
Pipe* findPipeByName(PipeList* list, const char* name);
Concat* findConcatByName(ConcatList* list, const char* name);
void printExecutionTree(TreeNode* node, int depth); 
// void freeMem(nodeDef *nodes, int nodeCount, pipeDef *pipes, int pipeCount, concatDef *concats, int concatCount,stderrDef *stderrs, int stderrCoun //fileDef *files, int fileCount
// ); 
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <flowfile>\n", argv[0]);
        return 1;
    }
    ListContainer listContainer = {NULL, NULL, NULL, NULL};
    Tree* executionTree = (Tree*) malloc(sizeof(Tree));
    executionTree->root = NULL;
    executionTree->size = 0;


    // --- Parse each section individually ---
    parseFile(argv[1], &listContainer);
    char* rootName = findRootNode(&listContainer);
    if (rootName != NULL){
        if (strncmp(rootName,"Node", strlen("Node")) == 0){
           printf("Its a node execute all node one by one\n");
           exit(1); 
        }
        createExecutionTree(&listContainer, executionTree, rootName);
        printExecutionTree(executionTree->root, 0);
        printf("here2\n");
    }else{
        perror("No root node found\n");
        exit(0);
    }
    // // --- Print out parsed data ---
    // printf("\n=== Parsed Nodes ===\n");
    for (int i = 0; i < listContainer.nodeList->size; i++) {
        printf("Node %d:\n", i + 1);
        printf("  Name: %s\n", listContainer.nodeList->list[i]->name);
        printf("  Command: %s\n", listContainer.nodeList->list[i]->command ?  listContainer.nodeList->list[i]->command : "(none)");
    }

    printf("\n=== Parsed Pipes ===\n");
    for (int i = 0; i < listContainer.pipeList->size; i++) {
        printf("Pipe %d:\n", i + 1);
        printf("  Name: %s\n", listContainer.pipeList->list[i]->name);
        printf("  From: %s\n", listContainer.pipeList->list[i]->from ? listContainer.pipeList->list[i]->from: "(none)");
        printf("  To: %s\n", listContainer.pipeList->list[i]->to ? listContainer.pipeList->list[i]->to : "(none)");
    }

    printf("\n=== Parsed Concatenations ===\n");
    for (int i = 0; i < listContainer.concatList->size; i++) {
        printf("Concat %d:\n", i + 1);
        printf("  Name: %s\n", listContainer.concatList->list[i]->name);
        printf("  Parts (%d):\n", listContainer.concatList->list[i]->partCount);
        for (int j = 0; j < listContainer.concatList->list[i]->partCount; j++) {
            printf("    Part %d: %s\n", j, listContainer.concatList->list[i]->parts[j]);
        }
    }

    // printf("\n=== Parsed Stderr Redirections ===\n");
    // for (int i = 0; i < stderrCount; i++) {
    //     printf("Stderr %d:\n", i + 1);
    //     printf("  Name: %s\n", stderrs[i].name);
    //     printf("  From: %s\n", stderrs[i].from ? stderrs[i].from : "(none)");
    // }
    /*
    printf("\n=== Parsed Files ===\n");
    for (int i = 0; i < fileCount; i++) {
        printf("File %d:\n", i + 1);
        printf("  Name: %s\n", files[i].name);
        printf("  FileName: %s\n", files[i].fileName ? files[i].fileName : "(none)");
    }
    */


    // --- Cleanup all allocated memory ---
    // freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount); //, files, fileCount);

    printf("\nAll memory freed successfully.\n");
    return 0;
}
void printExecutionTree(TreeNode* node, int depth) {
    if (!node) return;

    for (int i = 0; i < depth; i++) printf("  ");
    switch (node->type) {
        case NODE_TYPE:
            printf("Node: %s (%s)\n", node->data.node->name, node->data.node->command);
            break;
        case PIPE_TYPE:
            printf("Pipe: %s (from=%s, to=%s)\n",
                node->data.pipe->name,
                node->data.pipe->from,
                node->data.pipe->to);
            break;
        case CONCAT_TYPE:
            printf("Concat: %s (%d parts)\n",
                node->data.concat->name,
                node->data.concat->partCount);
            break;
        default:
            printf("Unknown type\n");
    }

    printExecutionTree(node->left, depth + 1);
    printExecutionTree(node->right, depth + 1);
}

Node* findNodeByName(NodeList* list, const char* name) {
    for (int i = 0; list && i < list->size; i++)
        if (strcmp(list->list[i]->name, name) == 0)
            return list->list[i];
    return NULL;
}

Pipe* findPipeByName(PipeList* list, const char* name) {
    for (int i = 0; list && i < list->size; i++)
        if (strcmp(list->list[i]->name, name) == 0)
            return list->list[i];
    return NULL;
}

Concat* findConcatByName(ConcatList* list, const char* name) {
    for (int i = 0; list && i < list->size; i++)
        if (strcmp(list->list[i]->name, name) == 0)
            return list->list[i];
    return NULL;
}

TreeNode* buildTreeNode(ListContainer* listContainer, char* name) {
    char* typeStr = getNodeType(listContainer, name);
    if (!typeStr) {
        fprintf(stderr, "Unknown node type for: %s\n", name);
        return NULL;
    }

    TreeNode* node = (TreeNode*) malloc(sizeof(TreeNode));
    node->left = NULL;
    node->right = NULL;

    if (strcmp(typeStr, "Node") == 0) {
        node->type = NODE_TYPE;
        node->data.node = findNodeByName(listContainer->nodeList, name);
        return node;
    }

    if (strcmp(typeStr, "Pipe") == 0) {
        node->type = PIPE_TYPE;
        node->data.pipe = findPipeByName(listContainer->pipeList, name);
        if (!node->data.pipe) return NULL;

        // recursively connect from/to
        if (node->data.pipe->from)
            node->left = buildTreeNode(listContainer, node->data.pipe->from);
        if (node->data.pipe->to)
            node->right = buildTreeNode(listContainer, node->data.pipe->to);

        return node;
    }

    if (strcmp(typeStr, "Concat") == 0) {
        node->type = CONCAT_TYPE;
        node->data.concat = findConcatByName(listContainer->concatList, name);
        if (!node->data.concat) return NULL;

        Concat* concat = node->data.concat;
        if (concat->partCount > 0) {
            node->left = buildTreeNode(listContainer, concat->parts[0]);
            TreeNode* current = node->left;
            for (int i = 1; i < concat->partCount; i++) {
                current->right = buildTreeNode(listContainer, concat->parts[i]);
                current = current->right;
            }
        }
        return node;
    }

    return NULL;
}

void createExecutionTree(ListContainer* listContainer, Tree* tree, char* rootName)
{
    if (!tree) {
        tree = (Tree*) malloc(sizeof(Tree));
        tree->root = NULL;
        tree->size = 0;
    }

    tree->root = buildTreeNode(listContainer, rootName);
    tree->size = 1;
}
char* getNodeType(ListContainer* listContainer, char* name)
{

    for (int i = 0; i < listContainer->nodeList->size; i++){ //add all parts name into array
        if (strcmp(name, listContainer->nodeList->list[i]->name) == 0)
            return "Node"; 
    }
    for (int i = 0; i < listContainer->pipeList->size; i++){ //add all from/to names into array
        if (strcmp(name, listContainer->pipeList->list[i]->name) == 0)
            return "Pipe"; 
    }
    for (int i = 0; i < listContainer->concatList->size; i++){ //add all parts name into array
        if (strcmp(name, listContainer->concatList->list[i]->name) == 0)
            return "Concat"; 
    }
    return NULL;
}

char* findRootNode(ListContainer* listContainer)
{
    int capacity = 10;
    int size = 0;
    char** refList = (char **) malloc(sizeof(char *) * capacity);
    for (int i = 0; i < listContainer->pipeList->size; i++){ //add all from/to names into array
        if (size == capacity){
            capacity *=2;
            char** tmp = (char **) realloc(refList, sizeof(char *) * capacity);
            if (!tmp){
                perror("Failed to realloc refList\n");
                exit(1);
            }
            refList = tmp;
        }
        if (listContainer->pipeList->list[i]->from)
            refList[size++] = listContainer->pipeList->list[i]->from;
        if (listContainer->pipeList->list[i]->to)
            refList[size++] = listContainer->pipeList->list[i]->to;
    }
    for (int i = 0; i < listContainer->concatList->size; i++){ //add all parts name into array
        for (int j = 0; j < listContainer->concatList->list[i]->partSize; j++){
            if (size == capacity){
                capacity *=2;
                char** tmp = (char **) realloc(refList, sizeof(char *) * capacity);
                if (!tmp){
                    perror("Failed to realloc refList\n");
                    exit(1);
                }
                refList = tmp;
            }
            if (listContainer->concatList->list[i]->parts[j])
                refList[size++] = listContainer->concatList->list[i]->parts[j];
        }
    }
    char* rootName = NULL;
    for (int i = 0; i < listContainer->pipeList->size; i++){ //check for un-referenced pipes
        if (!isInRefList(refList, size, listContainer->pipeList->list[i]->name)){
            if (rootName == NULL){
                rootName = listContainer->pipeList->list[i]->name;
            }else{
                perror("Can't be two roots. Tree cycle\n");
                exit(1);
            }
        }
    }
    for (int i = 0; i < listContainer->concatList->size; i++){ //check for un-reference concats
        if (!isInRefList(refList, size, listContainer->concatList->list[i]->name)){
            if (rootName == NULL){
                rootName = listContainer->concatList->list[i]->name;
            }else{
                perror("Can't be two roots. Tree cycle\n");
                exit(1);
            }
        }
    }

    if (rootName == NULL){
        rootName = "Node"; 
        return rootName;
    } 
    return rootName;
}

bool isInRefList(char** refList, int size, char* name)
{
    for (int i = 0; i < size; i++){
        if (strncmp(refList[i], name, strlen(name)) == 0){
            return true;
        }
    }
    return false;
}
void parseFile(const char *filename, ListContainer* listContainer) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening flow file");
        exit(1);
    }

    char lineBuffer[512];
    while (fgets(lineBuffer, sizeof(lineBuffer), fp)) {
        lineBuffer[strcspn(lineBuffer, "\n")] = '\0';

        if (strlen(lineBuffer) == 0)
            continue;

        // --- NODE SECTION ---
        if (strncmp(lineBuffer, "node=", 5) == 0) {
            createNode(listContainer);
            saveNodeName(listContainer, lineBuffer);
        }
        // --- COMMAND SECTION ---
        else if (strncmp(lineBuffer, "command=", 8) == 0 && listContainer->nodeList->list + (listContainer->nodeList->size - 1)) {
            saveCmd(listContainer, lineBuffer);
        }

        // --- PIPE SECTION ---
        if (strncmp(lineBuffer, "pipe=", 5) == 0) {
            createPipe(listContainer);
            int idx = listContainer->pipeList->size;
            Pipe* newPipe = listContainer->pipeList->list[idx - 1];
            savePipeAttr(&(newPipe->name), lineBuffer, 5);
        }

        // --- PIPE FROM ---
        else if (strncmp(lineBuffer, "from=", 5) == 0 && listContainer->pipeList->list + (listContainer->pipeList->size - 1)) {
            int idx = listContainer->pipeList->size;
            Pipe* newPipe = listContainer->pipeList->list[idx - 1];
            savePipeAttr(&(newPipe->from), lineBuffer, 5);

        }

        // --- PIPE TO ---
        else if (strncmp(lineBuffer, "to=", 3) == 0 && listContainer->pipeList->list + (listContainer->pipeList->size - 1)) {
            int idx = listContainer->pipeList->size;
            Pipe* newPipe = listContainer->pipeList->list[idx - 1];
            savePipeAttr(&(newPipe->to), lineBuffer, 3);

        }
        // --- CONCAT SECTION ---
        if (strncmp(lineBuffer, "concatenate=", 12) == 0) {
            createConcat(listContainer);
            int idx = listContainer->concatList->size;
            Concat* newConcat = listContainer->concatList->list[idx - 1];
            saveConcatAttr(&(newConcat->name), lineBuffer, 12);
          

        }
        // --- PART COUNT SECTION ---
        else if (strncmp(lineBuffer, "parts=", 6) == 0 && listContainer->concatList->list + (listContainer->concatList->size - 1)) {
            int count = atoi(lineBuffer + 6);
            if (count > 0) {
                int idx = listContainer->concatList->size;
                Concat* newConcat = listContainer->concatList->list[idx - 1];
                newConcat->partCount = count;
            }
        }

        // --- INDIVIDUAL PARTS: part_0=..., part_1=..., etc. ---
        else if (strncmp(lineBuffer, "part_", 5) == 0 && listContainer->concatList->list + (listContainer->concatList->size - 1)) {
                char *startOfWord = strchr(lineBuffer, '=');
                if (startOfWord) {
                    saveParts(listContainer->concatList, startOfWord + 1);
                }
        }
    }
    fclose(fp);
}
void saveParts(ConcatList* concatList, char* startOfWord)
{
    int idx = concatList->size;
    Concat* concatNode = concatList->list[idx - 1];
    if (!concatNode->parts){
        perror("parts was not allocated\n");
        exit(1);
    }
    if (concatNode->partCount < concatList->list[idx - 1]->partSize){
        perror("there are more parts than parts count\n");
        exit(1);
    }
    if (concatNode->partSize == concatNode->partCapacity){ //Need to allocate more space for new Node
        concatNode->partCapacity *= 2;
        char** tmp = (char **) realloc(concatNode->parts, sizeof(char *) * concatNode->partCapacity);
        if (!tmp){
            perror("Failed to realloc. Maybe low on space\n");
            exit(1);
        }
        concatNode->parts = tmp;
    }
    concatNode->parts[concatNode->partSize] = (char *) malloc(strlen(startOfWord) + 1);
    strcpy(concatNode->parts[concatNode->partSize], startOfWord);
    concatNode->partSize += 1;

}
void saveConcatAttr(char** concatAttr, char* lineBuffer, int offset)
{
    *concatAttr = (char *) malloc(strlen(lineBuffer + offset) + 1);
    strcpy(*concatAttr, lineBuffer + offset);
}
void createConcat(ListContainer* listContainer)
{
  if (!listContainer->concatList){ //First insert of List
    listContainer->concatList = (ConcatList *) malloc(sizeof(ConcatList));
    ConcatList* concatList = listContainer->concatList;
    concatList->size = 0;
    concatList->capacity = 10;
    concatList->list = (Concat **) malloc(sizeof(Concat *) * 10);     
  }
    ConcatList* concatList = listContainer->concatList;
    if (concatList->size == concatList->capacity){ //Need to allocate more space for new Node
        concatList->capacity *= 2;
        Concat** tmp = (Concat**) realloc(concatList->list, sizeof(Concat *) * concatList->capacity);
        if (!tmp){
            perror("Failed to realloc. Maybe low on space\n");
            exit(1);
        }
        concatList->list = tmp;
    }
    concatList->list[concatList->size] = (Concat *) malloc(sizeof(Concat));  
    concatList->list[concatList->size]->parts = (char **) malloc(sizeof(char *) * 10);   
    concatList->list[concatList->size]->partSize = 0; 
    concatList->list[concatList->size]->partCapacity = 10; 
    listContainer->concatList->size += 1;
}
void createNode(ListContainer* listContainer)
{
  if (!listContainer->nodeList){ //First insert of List
    listContainer->nodeList = (NodeList *) malloc(sizeof(NodeList));
    NodeList* nodeList = listContainer->nodeList;
    nodeList->size = 0;
    nodeList->capacity = 10;
    nodeList->list = (Node **) malloc(sizeof(Node *) * 10);     
    }
    
    NodeList* nodeList = listContainer->nodeList;
    if (nodeList->size == nodeList->capacity){ //Need to allocate more space for new Node
        nodeList->capacity *= 2;
        Node** tmp = (Node**) realloc(nodeList->list, sizeof(Node*) * nodeList->capacity);
        if (!tmp){
            perror("Failed to realloc. Maybe low on space\n");
            exit(1);
        }
        nodeList->list = tmp;
    }
    nodeList->list[nodeList->size] = (Node *) malloc(sizeof(Node));     
    listContainer->nodeList->size += 1;
}

void saveNodeName(ListContainer* listContainer, char* lineBuffer)
{
    NodeList* nodeList = listContainer->nodeList;
    Node* newNode = nodeList->list[nodeList->size - 1];
    newNode->name = (char *) malloc(strlen(lineBuffer + 5) + 1);
    strcpy(newNode->name, lineBuffer + 5);
}
void saveCmd(ListContainer* listContainer, char* lineBuffer)
{
    int idx = listContainer->nodeList->size;
    listContainer->nodeList->list[idx - 1]->command = (char * ) malloc(strlen(lineBuffer + 8) + 1);
    strcpy(listContainer->nodeList->list[idx - 1]->command, lineBuffer + 8);
}

void createPipe(ListContainer* listContainer)
{
    if (!listContainer->pipeList){ //First insert of List
        listContainer->pipeList = (PipeList *) malloc(sizeof(PipeList));
        PipeList* pipeList = listContainer->pipeList;
        pipeList->size = 0;
        pipeList->capacity = 10;
        pipeList->list = (Pipe **) malloc(sizeof(Pipe *) * 10);     
    }   
    PipeList* pipeList = listContainer->pipeList;
    if (pipeList->size == pipeList->capacity){ //Need to allocate more space for new Node
        pipeList->capacity *= 2;
        Pipe** tmp = (Pipe**) realloc(pipeList->list, sizeof(Pipe*) * pipeList->capacity);
        if (!tmp){
            perror("Failed to realloc. Maybe low on space\n");
            exit(1);
        }
        pipeList->list = tmp;
    } 
    pipeList->list[pipeList->size] = (Pipe *) malloc(sizeof(Pipe));     
    listContainer->pipeList->size += 1;
}
void savePipeAttr(char** pipeAttr, char* lineBuffer, int offset)
{
    *pipeAttr = (char *) malloc(strlen(lineBuffer + offset) + 1);
    strcpy(*pipeAttr, lineBuffer + offset);
}

// void parseFlowStderr(const char *filename, stderrDef **stderrs, int *stderrCount) {
//     FILE *fp = fopen(filename, "r");
//     if (!fp) {
//         perror("Error opening flow file");
//         exit(1);
//     }

//     char lineBuffer[512];
//     stderrDef *currentStderr = NULL;

//     while (fgets(lineBuffer, sizeof(lineBuffer), fp)) {
//         lineBuffer[strcspn(lineBuffer, "\n")] = '\0';

//         if (strlen(lineBuffer) == 0)
//             continue;

//         // --- STDERR SECTION ---
//         if (strncmp(lineBuffer, "stderr=", 7) == 0) {
//             *stderrs = realloc(*stderrs, (*stderrCount + 1) * sizeof(stderrDef));
//             if (!*stderrs) {
//                 perror("realloc failed for stderrs");
//                 fclose(fp);
//                 exit(1);
//             }

//             currentStderr = &(*stderrs)[*stderrCount];
//             currentStderr->name = malloc(strlen(lineBuffer + 7) + 1);
//             strcpy(currentStderr->name, lineBuffer + 7);
//             currentStderr->from = NULL;

//             (*stderrCount)++;
//         }

//         // --- FROM SECTION ---
//         else if (strncmp(lineBuffer, "from=", 5) == 0 && currentStderr) {
//             currentStderr->from = malloc(strlen(lineBuffer + 5) + 1);
//             strcpy(currentStderr->from, lineBuffer + 5);
//         }   
//     }
//      fclose(fp);
// }


// void freeMem(
//     nodeDef *nodes, int nodeCount,
//     pipeDef *pipes, int pipeCount,
//     concatDef *concats, int concatCount,
//     stderrDef *stderrs, int stderrCount
//     //fileDef *files, int fileCount
// ) {
//     // --- Free nodes ---
//     for (int i = 0; i < nodeCount; i++) {
//         free(nodes[i].name);
//         free(nodes[i].command);
//     }
//     free(nodes);

//     // --- Free pipes ---
//     for (int i = 0; i < pipeCount; i++) {
//         free(pipes[i].name);
//         free(pipes[i].from);
//         free(pipes[i].to);
//     }
//     free(pipes);

//     // --- Free concats ---
//     for (int i = 0; i < concatCount; i++) {
//         free(concats[i].name);

//         if (concats[i].parts) {
//             int count = concats[i].partCount;
//             for (int j = 0; j < count; j++) {
//                 free(concats[i].parts[j]);
//             }
//             free(concats[i].parts);
//         }
//     }
//     free(concats);

//     // --- Free stderr mappings ---
//     for (int i = 0; i < stderrCount; i++) {
//         free(stderrs[i].name);
//         free(stderrs[i].from);
//     }
//     free(stderrs);

//     // --- Free files ---
//     /*
//     for (int i = 0; i < fileCount; i++) {
//         free(files[i].name);
//         free(files[i].fileName);
//     }
//     free(files);
//     */
// }

