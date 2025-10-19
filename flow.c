#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include<stdbool.h>
#include<sys/types.h>
#include<sys/wait.h>

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
typedef enum {BLOCK_NONE, BLOCK_NODE, BLOCK_PIPE, BLOCK_CONCAT, BLOCK_STDERR} BlockType;

typedef union {
    Node* node;
    Pipe* pipe;
    Concat* concat;
    Stderr* stderr;
} NodeData;

typedef struct TreeNode {
    NodeType type;
    NodeData data;
    int visited;
    int inStack;
    struct TreeNode* left;
    struct TreeNode* right;
} TreeNode;

typedef struct Tree{
    TreeNode* root;
    int size;
} Tree;

typedef struct {
    char** names;        // node names
    TreeNode** nodes;    // pointers to TreeNode
    int size;
    int capacity;
} TreeNodeList;

/*
typedef struct {
    char *name;
    int *fileName;
} fileDef;
 */

// Core Functions
void parseFile(const char *filename, ListContainer* listContainer);
TreeNode* createExecutionTree(ListContainer* listContainer, TreeNodeList* seenNodeList, char* rootName);
void executeTree(TreeNode* node, int inputFile, int outputFile);
// Tree Node Creating Helper Functions
void createNode(ListContainer* listContainer);
void createPipe(ListContainer* listContainer);
void createConcat(ListContainer* listContainer);
void createStderr(ListContainer* listContainer);
// Node Attribute Save Helper Functions
void saveNodeName(ListContainer* listContainer, char* lineBuffer);
void saveNodeCmd(ListContainer* listContainer, char* lineBuffer);
void savePipeAttr(char** pipeAttr, char* lineBuffer, int offset);
void saveConcatAttr(char** concatAttr, char* lineBuffer, int offset);
void saveParts(ConcatList* concatList, char* startOfWord);
void saveStderrName(ListContainer* listContainer, char* lineBuffer);
void saveStderrFrom(ListContainer* listContainer, char* lineBuffer);
// Node Atrribute Get Helper Functions
char* getNodeType(ListContainer* listContainer, char* name);
Node* findNodeByName(NodeList* list, const char* name);
Pipe* findPipeByName(PipeList* list, const char* name);
Concat* findConcatByName(ConcatList* list, const char* name);
Stderr* findStderrByName(StderrList* list, const char* name);
// void printExecutionTree(TreeNode* node, int depth); 
// Detect Cycles & helpers methods to keep track of existing nodes to not keep allocating same node infinetly 
int detectCycle(TreeNode* node);
TreeNode* findTreeNode(TreeNodeList* list, const char* name);
void addTreeNode(TreeNodeList* list, const char* name, TreeNode* node);
// Release all memory
void freeMem(ListContainer* listOfLists, TreeNodeList* listOfNodes, TreeNode* node);
void freeListsMems(ListContainer* listOfLists, TreeNodeList* listOfNodes);
void freeTree(TreeNode* node);
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
    TreeNodeList* list = (TreeNodeList *) malloc(sizeof(TreeNodeList));
    list->names = (char**) malloc(sizeof(char*) * 10);
    list->nodes = (TreeNode**) malloc(sizeof(TreeNode*) * 10);
    list->size = 0;
    list->capacity = 10;
    
    // --- Parse each section individually ---
    parseFile(argv[1], &listContainer); //Read input file line by line. Then group all NODE Types(Node, Pipe, Concat, etc) individually. Finally store them inside listContainer
    if (argv[2] != NULL){

        executionTree->root = createExecutionTree(&listContainer, list, argv[2]);

    if (detectCycle(executionTree->root)) {
        fprintf(stderr, "Error: circular dependency detected!\n");
        exit(1);
    } else {
        printf("No cycles found.\n");
    }
        executionTree->size = 1;
        executeTree(executionTree->root, STDIN_FILENO, STDOUT_FILENO);
        // printExecutionTree(executionTree->root, 2);
    }else{
        perror("Incorrect input\n");
        exit(1);
    }
   
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

// Core Functions
void parseFile(const char *filename, ListContainer* listContainer) 
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening flow file");
        exit(1);
    }

    BlockType currentBlock = BLOCK_NONE;
    char lineBuffer[512];
    while (fgets(lineBuffer, sizeof(lineBuffer), fp)) { //fgets read n-1 and add a '\0' char. stops reading at '\n', reached N bytes, or EOF. Keeps word\n, so we need to remove it
        lineBuffer[strcspn(lineBuffer, "\n")] = '\0'; // looks for newline index and if not re-inserts \0 at end.
    
        if (strlen(lineBuffer) == 0) {
            currentBlock = BLOCK_NONE;
            continue;
        }

        // --- BLOCK HEADERS ---
        if (strncmp(lineBuffer, "node=", 5) == 0) {
            createNode(listContainer);
            saveNodeName(listContainer, lineBuffer);
            currentBlock = BLOCK_NODE;
            continue;
        } else if (strncmp(lineBuffer, "pipe=", 5) == 0) {
            createPipe(listContainer);
            int idx = listContainer->pipeList->size;
            Pipe* newPipe = listContainer->pipeList->list[idx - 1];
            savePipeAttr(&(newPipe->name), lineBuffer, 5);
            currentBlock = BLOCK_PIPE;
            continue;
        } else if (strncmp(lineBuffer, "concatenate=", 12) == 0) {
            createConcat(listContainer);
            int idx = listContainer->concatList->size;
            Concat* newConcat = listContainer->concatList->list[idx - 1];
            saveConcatAttr(&(newConcat->name), lineBuffer, 12);
            currentBlock = BLOCK_CONCAT;
            continue;
        } else if (strncmp(lineBuffer, "stderr=", 7) == 0) {
            createStderr(listContainer);
            saveStderrName(listContainer, lineBuffer);
            currentBlock = BLOCK_STDERR;
            continue;
        }

        // --- BLOCK CONTENTS ---
        switch (currentBlock) {
            case BLOCK_NODE:
                if (strncmp(lineBuffer, "command=", 8) == 0) {
                    saveNodeCmd(listContainer, lineBuffer);
                }
                break;

            case BLOCK_PIPE:
                if (strncmp(lineBuffer, "from=", 5) == 0) {
                    int idx = listContainer->pipeList->size;
                    Pipe* newPipe = listContainer->pipeList->list[idx - 1];
                    savePipeAttr(&(newPipe->from), lineBuffer, 5);
                } else if (strncmp(lineBuffer, "to=", 3) == 0) {
                    int idx = listContainer->pipeList->size;
                    Pipe* newPipe = listContainer->pipeList->list[idx - 1];
                    savePipeAttr(&(newPipe->to), lineBuffer, 3);
                }
                break;

            case BLOCK_CONCAT:
                if (strncmp(lineBuffer, "parts=", 6) == 0) {
                    int count = atoi(lineBuffer + 6);
                    if (count > 0) {
                        int idx = listContainer->concatList->size;
                        Concat* newConcat = listContainer->concatList->list[idx - 1];
                        newConcat->partCount = count;
                    }
                } else if (strncmp(lineBuffer, "part_", 5) == 0) {
                    char* start = strchr(lineBuffer, '=');
                    if (start) {
                        saveParts(listContainer->concatList, start + 1);
                    }
                }
                break;

            case BLOCK_STDERR:
                if (strncmp(lineBuffer, "from=", 5) == 0) {
                    saveStderrFrom(listContainer, lineBuffer);
                }
                break;

            case BLOCK_NONE:
            default:
                // ignore stray lines outside blocks
                break;
        }
    }
    fclose(fp);
}
TreeNode* createExecutionTree(ListContainer* listContainer, TreeNodeList* seenNodesList, char* name)
{
    // Check if this node is already in the recursion stack (true cycle)
    TreeNode* existing = findTreeNode(seenNodesList, name);
    if (existing && existing->inStack) {
        fprintf(stderr, "Cycle detected at node: %s\n", name);
        return NULL;
    }

    char* typeStr = getNodeType(listContainer, name);
    if (!typeStr) {
        fprintf(stderr, "Unknown node type for: %s\n", name);
        return NULL;
    }

    TreeNode* node = (TreeNode*) malloc(sizeof(TreeNode));
    node->left = NULL;
    node->right = NULL;
    node->visited = 0;
    node->inStack = 1; // mark in recursion stack

    addTreeNode(seenNodesList, name, node);  // store before recursion

    if (strcmp(typeStr, "Node") == 0) {
        node->type = NODE_TYPE;
        node->data.node = findNodeByName(listContainer->nodeList, name);
    } else if (strcmp(typeStr, "Pipe") == 0) {
        node->type = PIPE_TYPE;
        node->data.pipe = findPipeByName(listContainer->pipeList, name);
        if (!node->data.pipe) return NULL;

        if (node->data.pipe->from)
            node->left = createExecutionTree(listContainer, seenNodesList, node->data.pipe->from);
        if (node->data.pipe->to)
            node->right = createExecutionTree(listContainer, seenNodesList, node->data.pipe->to);

    } else if (strcmp(typeStr, "Concat") == 0) {
        node->type = CONCAT_TYPE;
        node->data.concat = findConcatByName(listContainer->concatList, name);
        if (!node->data.concat) return NULL;

        if (node->data.concat->partCount > 0) {
            node->left = createExecutionTree(listContainer, seenNodesList, node->data.concat->parts[0]);
            TreeNode* current = node->left;

            for (int i = 1; i < node->data.concat->partCount; i++) {
                TreeNode* next = createExecutionTree(listContainer, seenNodesList, node->data.concat->parts[i]);
                current->right = next;
                current = next;
            }
        }

    } else if (strcmp(typeStr, "Stderr") == 0) {
        node->type = STDERR_TYPE;
        node->data.stderr = findStderrByName(listContainer->stderrList, name);
        if (!node->data.stderr) return NULL;

        if (node->data.stderr->from)
            node->left = createExecutionTree(listContainer, seenNodesList, node->data.stderr->from);
    }

    node->inStack = 0; // done with this node
    node->visited = 1;
    return node;
}

void executeTree(TreeNode* node, int inputFile, int outputFile) {
    if (!node) {
        return;
    }

    if (node->type == NODE_TYPE) {
        int capacity = 10, argc = 0;
        char **argv = (char**) malloc(sizeof(char*) * capacity);
        char *p = node->data.node->command;

        while (*p) {
            while (*p && *p == ' ') p++;
            if (!*p) break;

            char *start;
            char quote = 0;
            if (*p == '\'' || *p == '"') {
                quote = *p++;
                start = p;
                while (*p && *p != quote) p++;
            } else {
                start = p;
                while (*p && *p != ' ') p++;
            }

            int len = p - start;
            char *arg = (char *) malloc(len + 1);
            strncpy(arg, start, len);
            arg[len] = '\0';

            if (argc == capacity - 1) {
                capacity *= 2;
                argv = (char **) realloc(argv, sizeof(char*) * capacity);
            }
            argv[argc++] = arg;
            if (*p) p++;
        }
        argv[argc] = NULL;


        if (inputFile != STDIN_FILENO) { dup2(inputFile, STDIN_FILENO); close(inputFile); }
        if (outputFile != STDOUT_FILENO) { dup2(outputFile, STDOUT_FILENO); close(outputFile); }

        execvp(argv[0], argv);
        perror("execvp failed");
        for (int i = 0; i < argc; i++) free(argv[i]);
        free(argv);
        exit(1);

    } else if (node->type == PIPE_TYPE) {
        int fd[2]; pipe(fd);

        pid_t pid_left = fork();
        if (pid_left == 0) {
            close(fd[0]);
            executeTree(node->left, inputFile, fd[1]);
            close(fd[1]);
            exit(0);
        }

        pid_t pid_right = fork();
        if (pid_right == 0) {
            close(fd[1]);
            executeTree(node->right, fd[0], outputFile);
            close(fd[0]);
            exit(0);
        }

        close(fd[0]); close(fd[1]);
        waitpid(pid_left, NULL, 0);
        waitpid(pid_right, NULL, 0);

    }else if (node->type == CONCAT_TYPE) {
    TreeNode* current = node->left;
    int partNum = 0;
    int totalParts = node->data.concat->partCount;

    while (current && partNum < totalParts) {

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed for CONCAT part");
            exit(1);
        } else if (pid == 0) {
            // child executes the current part
            executeTree(current, STDIN_FILENO, STDOUT_FILENO);
            exit(0);
        } else {
            int status;
            waitpid(pid, &status, 0);
        }

        current = current->right;
        partNum++;
    }
}else if (node->type == STDERR_TYPE) {
        pid_t pid = fork();
        if (pid == 0) {
            dup2(STDOUT_FILENO, STDERR_FILENO);
            executeTree(node->left, inputFile, outputFile);
            exit(0);
        }
        waitpid(pid, NULL, 0);
    }
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
        case STDERR_TYPE:
            printf("Stderr: %s (from=%s)\n",
                node->data.stderr->name,
                node->data.stderr->from);
            break;
        default:
            printf("Unknown type\n");
    }

    printExecutionTree(node->left, depth + 1);
    printExecutionTree(node->right, depth + 1);
}
// TreeNode Creating function
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
void createStderr(ListContainer* listContainer)
{
    if (!listContainer->stderrList){ //new list created
        listContainer->stderrList =  (StderrList*) malloc(sizeof(StderrList));//size of what stderrList ptr points to
        listContainer->stderrList->size = 0;
        listContainer->stderrList->capacity = 10;
        listContainer->stderrList->list = (Stderr**) malloc(sizeof(Stderr *) * listContainer->stderrList->capacity);//size of what the first element of the list ptr point to
    }
    StderrList* stderrList = listContainer->stderrList;
    if (stderrList->size == stderrList->capacity){
        stderrList->capacity *= 2;
        Stderr** tmp = (Stderr**) realloc(stderrList->list, sizeof(Stderr *) * stderrList->capacity);
        if (!tmp){
            perror("failed to realloc stderr list\n");
            exit(1);
        }
        listContainer->stderrList->list = tmp;

    }
    stderrList->list[stderrList->size++] = (Stderr*) malloc(sizeof(Stderr));
}
// Getter functions to extract attributes from each NODE TYPE
Node* findNodeByName(NodeList* list, const char* name) {
    if (!list) return NULL; 
    for (int i = 0; i < list->size; i++)
        if (strcmp(list->list[i]->name, name) == 0){
            return list->list[i];
        }
    return NULL;
}
Pipe* findPipeByName(PipeList* list, const char* name) {
    if (!list) return NULL; 
    for (int i = 0; i < list->size; i++)
        if (strcmp(list->list[i]->name, name) == 0)
            return list->list[i];
    return NULL;
}
Concat* findConcatByName(ConcatList* list, const char* name) {
    if (!list) return NULL; 
    for (int i = 0; i < list->size; i++)
        if (strcmp(list->list[i]->name, name) == 0)
            return list->list[i];
    return NULL;
}
Stderr* findStderrByName(StderrList* list, const char* name) {
    if (!list) return NULL; 
    for (int i = 0; i < list->size; i++)
        if (strcmp(list->list[i]->name, name) == 0)
            return list->list[i];
    return NULL;
}
char* getNodeType(ListContainer* listContainer, char* name)
{
    if (listContainer->nodeList) {
        for (int i = 0; i < listContainer->nodeList->size; i++)
            if (strcmp(name, listContainer->nodeList->list[i]->name) == 0)
                return "Node"; 
    }
    if (listContainer->pipeList) {
        for (int i = 0; i < listContainer->pipeList->size; i++)
            if (strcmp(name, listContainer->pipeList->list[i]->name) == 0)
                return "Pipe"; 
    }
    if (listContainer->concatList) {
        for (int i = 0; i < listContainer->concatList->size; i++)
            if (strcmp(name, listContainer->concatList->list[i]->name) == 0)
                return "Concat"; 
    }
    if (listContainer->stderrList) {
        for (int i = 0; i < listContainer->stderrList->size; i++)
            if (strcmp(name, listContainer->stderrList->list[i]->name) == 0)
                return "Stderr"; 
    }
    return NULL;
}
// Setter functions to save values into each NODE TYPE
void saveNodeName(ListContainer* listContainer, char* lineBuffer)
{
    NodeList* nodeList = listContainer->nodeList;
    Node* newNode = nodeList->list[nodeList->size - 1];
    newNode->name = (char *) malloc(strlen(lineBuffer + 5) + 1);
    strcpy(newNode->name, lineBuffer + 5);
}
void saveNodeCmd(ListContainer* listContainer, char* lineBuffer)
{
    int idx = listContainer->nodeList->size;
    listContainer->nodeList->list[idx - 1]->command = (char * ) malloc(strlen(lineBuffer + 8) + 1);
    strcpy(listContainer->nodeList->list[idx - 1]->command, lineBuffer + 8);
}
void savePipeAttr(char** pipeAttr, char* lineBuffer, int offset)
{
    *pipeAttr = (char *) malloc(strlen(lineBuffer + offset) + 1);
    strcpy(*pipeAttr, lineBuffer + offset);
}
void saveConcatAttr(char** concatAttr, char* lineBuffer, int offset)
{
    *concatAttr = (char *) malloc(strlen(lineBuffer + offset) + 1);
    strcpy(*concatAttr, lineBuffer + offset);
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
    concatNode->parts[concatNode->partSize] = (char *) malloc(strlen(startOfWord) + 1); //strlen does not count \0 as a char. We add +1 and let strcpy copy it from startOfWord
    strcpy(concatNode->parts[concatNode->partSize], startOfWord);
    concatNode->partSize += 1;

}
void saveStderrName(ListContainer* listContainer, char* lineBuffer)
{
    Stderr** stderrList = listContainer->stderrList->list;
    Stderr* newNode = stderrList[listContainer->stderrList->size - 1];
    newNode->name = (char*) malloc(strlen(lineBuffer + 7) + 1);
    strcpy(newNode->name, lineBuffer + 7);
}
void saveStderrFrom(ListContainer* listContainer, char* lineBuffer)
{
    int idx = listContainer->stderrList->size; 
    Stderr* newNode = listContainer->stderrList->list[idx - 1];
    newNode->from = (char*) malloc(strlen(lineBuffer + 5) + 1);
    strcpy(newNode->from, lineBuffer + 5);
}
//Cycle Detection
int detectCycle(TreeNode* node) {
    if (!node) return 0;

    if (node->inStack) {
        fprintf(stderr, "Cycle detected at node!\n");
        return 1;
    }
    if (node->visited) return 0;

    node->visited = 1;
    node->inStack = 1;

    if (detectCycle(node->left)) return 1;
    if (detectCycle(node->right)) return 1;

    node->inStack = 0;  // done exploring this path
    return 0;
}
// Find existing node by name
TreeNode* findTreeNode(TreeNodeList* list, const char* name) {
    for (int i = 0; i < list->size; i++) {
        if (strcmp(list->names[i], name) == 0)
            return list->nodes[i];
    }
    return NULL;
}
// Add new node to list of SeenNodes
void addTreeNode(TreeNodeList* list, const char* name, TreeNode* node) {
    if (list->size == list->capacity) {
        list->capacity *= 2;
        list->names = (char **) realloc(list->names, sizeof(char*) * list->capacity);
        list->nodes = (TreeNode**) realloc(list->nodes, sizeof(TreeNode*) * list->capacity);
    }
    list->names[list->size] = strdup(name);
    list->nodes[list->size] = node;
    list->size++;
}
//Free all Memory :)
void freeMem(ListContainer* listOfLists, TreeNodeList* listOfNodes, TreeNode* node)
{
    freeListsMems(listOfLists, listOfNodes);

}
void freeListsMems(ListContainer* listOfLists, TreeNodeList* listOfNodes)
{
    for (int i = 0; i < listOfLists->nodeList->size; i++){
        Node** nodeList = listOfLists->nodeList->list;
        free(nodeList[i]->name);
        free(nodeList[i]->command);
    }
    free(listOfLists->nodeList);
    for (int i = 0; i < listOfLists->pipeList->size; i++){
        Pipe** pipeList = listOfLists->pipeList->list;
        free(pipeList[i]->name);
        free(pipeList[i]->from);
        free(pipeList[i]->to);
    }
    free(listOfLists->pipeList);
    for (int i = 0; i < listOfLists->concatList->size; i++){
        Concat** concatList = listOfLists->concatList->list;
        free(concatList[i]->name);
        for(int j = 0; j < concatList[i]->partSize; j++){
            free(concatList[i]->parts[j]);
        }
    }
    free(listOfLists->concatList);
    for (int i = 0; i < listOfLists->stderrList->size; i++){
        Stderr** stderrList = listOfLists->stderrList->list;
        free(stderrList[i]->name);
        free(stderrList[i]->from);
    }
    free(listOfLists->stderrList);
    free(listOfLists);

    for (int i = 0; i < listOfNodes->size; i++){
        free(listOfNodes->names[i]);
        free(listOfNodes->nodes[i]);
    }
    free(listOfNodes);
}
void freeTree(TreeNode* node)
{
    if (!node) return;

    freeTree(node->left);
    freeTree(node->right);

    free(node);
}