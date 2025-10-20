#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_FLOW_DEPTH 64
#define MAX_FORK_LIMIT 50

typedef struct {
    char *name;
    char *command;
} nodeDef;

typedef struct {
    char *name;
    char *from;
    char*to;
} pipeDef;

typedef struct {
    char *name;
    int partCount;
    char **parts;
} concatDef;

typedef struct {
    char *name;
    char *from;
} stderrDef;

typedef struct {
    char *name;
    char *fileName;
} fileDef;

void freeMem(nodeDef *nodes, int nodeCount, pipeDef *pipes, int pipeCount, concatDef *concats, int concatCount, stderrDef *stderrs, int stderrCount, fileDef* files, int fileCount);
void parseFlowFile(const char *filename, nodeDef **nodes, int *nodeCount, pipeDef **pipes, int *pipeCount, concatDef **concats, int *concatCount, stderrDef **stderrs, int *stderrCount, fileDef **files, int *fileCount);
int directivePresent (const char *directive, nodeDef *nodes, int nodeCount, pipeDef *pipes, int pipeCount, concatDef *concats, int concatCount, stderrDef *stderrs, int stderrCount, fileDef *files, int fileCount);
char **splitCommand(const char *command);
void freeArgs(char **args);
void executeFlow(const char *blockName, nodeDef *nodes, int nodeCount, pipeDef *pipes, int pipeCount, concatDef *concats, int concatCount, stderrDef *stderrs, int stderrCount, fileDef *files, int fileCount);
int hasCycleUtil(const char *block, nodeDef *nodes, int nodeCount, pipeDef *pipes, int pipeCount, concatDef *concats, int concatCount, stderrDef *stderrs, int stderrCount, fileDef *files, int fileCount, char **visited, char **recStack, int depth);
int detectCycles( nodeDef *nodes, int nodeCount, pipeDef *pipes, int pipeCount, concatDef *concats, int concatCount, stderrDef *stderrs, int stderrCount, fileDef *files, int fileCount); 

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: ./flow <flowfile> <directive>\n");
        return 1;
    }
    
    // --- Allocate and initialize all structures ---
    nodeDef *nodes = NULL;
    pipeDef *pipes = NULL;
    concatDef *concats = NULL;
    stderrDef *stderrs = NULL;
    fileDef *files = NULL;
    int nodeCount = 0, pipeCount = 0, concatCount = 0, stderrCount = 0, fileCount = 0;
    
    parseFlowFile(argv[1], &nodes, &nodeCount, &pipes, &pipeCount, &concats, &concatCount, &stderrs, &stderrCount, &files, &fileCount);

    if (!directivePresent(argv[2], nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount)) {
        fprintf(stderr, "The directive provided is not present in the flow file.\n");
        freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount); 
        return 1;
    }

    // if no nodes, execute flow will enter infinite recursion
    if (nodeCount == 0) {
        fprintf(stderr, "No node directive present in the flow file.\n");
        freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount); 
        return 1;
    }
    
    if (detectCycles(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount)) {
        fprintf(stderr, "Flow validation failed: cyclic or invalid dependency found.\n");
        freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount); 
        return 1;
    }
    
    executeFlow(argv[2], nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount);

    freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount); 

    return 0;
}

void freeMem(nodeDef *nodes, int nodeCount, pipeDef *pipes, int pipeCount, concatDef *concats, int concatCount, stderrDef *stderrs, int stderrCount, fileDef* files, int fileCount) {
    if (!nodes && !pipes && !concats && !stderrs) 
        return;

    // --- Free nodes ---
    if (nodes) {
        for (int i = 0; i < nodeCount; i++) {
            free(nodes[i].name);
            free(nodes[i].command);
        }
        free(nodes);
    }
    // --- Free pipes ---
    if (pipes) {
        for (int i = 0; i < pipeCount; i++) {
            free(pipes[i].name);
            free(pipes[i].from);
            free(pipes[i].to);
        }
        free(pipes);
    }
    // --- Free concats ---
    if (concats) {
        for (int i = 0; i < concatCount; i++) {
            free(concats[i].name);

            if (concats[i].parts) {
                int count = concats[i].partCount;
                for (int j = 0; j < count; j++) {
                    free(concats[i].parts[j]);
                }
                free(concats[i].parts);
            }
        }
        free(concats);
    }
    // --- Free stderr mappings ---
    if (stderrs) {
        for (int i = 0; i < stderrCount; i++) {
            free(stderrs[i].name);
            free(stderrs[i].from);
        }
        free(stderrs);
    }
    // --- Free files ---
    if (files) {
        for (int i = 0; i < fileCount; i++) {
            free(files[i].name);
            free(files[i].fileName);
        }
        free(files);
    }
}

void parseFlowFile(const char *filename, nodeDef **nodes, int *nodeCount, pipeDef **pipes, int *pipeCount, concatDef **concats, int *concatCount, stderrDef **stderrs, int *stderrCount, fileDef **files, int *fileCount) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening flow file");
        exit(1);
    }

    char lineBuffer[512];
    nodeDef *currentNode = NULL;
    pipeDef *currentPipe = NULL;
    concatDef *currentConcat = NULL;
    stderrDef *currentStderr = NULL;
    fileDef *currentFile = NULL;

    int nodeCap = 0, pipeCap = 0, concatCap = 0, stderrCap = 0, fileCap = 0;
    
    while (fgets(lineBuffer, sizeof(lineBuffer), fp)) {
        lineBuffer[strcspn(lineBuffer, "\n")] = '\0';
        if (strlen(lineBuffer) == 0)
            continue;

        // --- NODE SECTION ---
        if (strncmp(lineBuffer, "node=", 5) == 0) {
            if (*nodes == NULL) {
                nodeCap = 1;
                *nodes = malloc(nodeCap * sizeof(nodeDef));
                if (!*nodes) {
                    perror("malloc failed for nodes");
                    fclose(fp);
                    freeMem(*nodes, *nodeCount, *pipes, *pipeCount, *concats, *concatCount, *stderrs, *stderrCount, *files, *fileCount);
                    exit(1);
                }
            } 
            else if (*nodeCount >= nodeCap) {
                nodeCap *= 2;
                nodeDef *tmp = realloc(*nodes, nodeCap * sizeof(nodeDef));
                if (!tmp) {
                    perror("realloc failed for nodes");
                    fclose(fp);
                    freeMem(*nodes, *nodeCount, *pipes, *pipeCount, *concats, *concatCount, *stderrs, *stderrCount, *files, *fileCount);
                    exit(1);
                }
                *nodes = tmp;
            }

            currentNode = &(*nodes)[(*nodeCount)++];
            currentNode->name = strdup(lineBuffer + 5);
            currentNode->command = NULL;
            continue;
        }

        if (strncmp(lineBuffer, "command=", 8) == 0 && currentNode) {
            currentNode->command = strdup(lineBuffer + 8);
            continue;
        }

        // --- PIPE SECTION ---
        if (strncmp(lineBuffer, "pipe=", 5) == 0) {
            if (*pipes == NULL) {
                pipeCap = 1;
                *pipes = malloc(pipeCap * sizeof(pipeDef));
                if (!*pipes) {
                    perror("malloc failed for pipes");
                    fclose(fp);
                    freeMem(*nodes, *nodeCount, *pipes, *pipeCount, *concats, *concatCount, *stderrs, *stderrCount, *files, *fileCount);
                    exit(1);
                }
            } 
            else if (*pipeCount >= pipeCap) {
                pipeCap *= 2;
                pipeDef *tmp = realloc(*pipes, pipeCap * sizeof(pipeDef));
                if (!tmp) {
                    perror("realloc failed for pipes");
                    fclose(fp);
                    freeMem(*nodes, *nodeCount, *pipes, *pipeCount, *concats, *concatCount, *stderrs, *stderrCount, *files, *fileCount);
                    exit(1);
                }
                *pipes = tmp;
            }

            currentPipe = &(*pipes)[(*pipeCount)++];
            currentPipe->name = strdup(lineBuffer + 5);
            currentPipe->from = NULL;
            currentPipe->to = NULL;
            continue;
        }

        if (strncmp(lineBuffer, "from=", 5) == 0 && currentPipe) {
            currentPipe->from = strdup(lineBuffer + 5);
            continue;
        }

        if (strncmp(lineBuffer, "to=", 3) == 0 && currentPipe) {
            currentPipe->to = strdup(lineBuffer + 3);
            continue;
        }

        // --- CONCAT SECTION ---
        if (strncmp(lineBuffer, "concatenate=", 12) == 0) {
            if (*concats == NULL) {
                concatCap = 1;
                *concats = malloc(concatCap * sizeof(concatDef));
                if (!*concats) {
                    perror("malloc failed for concats");
                    fclose(fp);
                    freeMem(*nodes, *nodeCount, *pipes, *pipeCount, *concats, *concatCount, *stderrs, *stderrCount, *files, *fileCount);
                    exit(1);
                }
            } 
            else if (*concatCount >= concatCap) {
                concatCap *= 2;
                concatDef *tmp = realloc(*concats, concatCap * sizeof(concatDef));
                if (!tmp) {
                    perror("realloc failed for concats");
                    fclose(fp);
                    freeMem(*nodes, *nodeCount, *pipes, *pipeCount, *concats, *concatCount, *stderrs, *stderrCount, *files, *fileCount);
                    exit(1);
                }
                *concats = tmp;
            }

            currentConcat = &(*concats)[(*concatCount)++];
            memset(currentConcat, 0, sizeof(concatDef));
            currentConcat->name = strdup(lineBuffer + 12);
            currentConcat->partCount = 0;
            currentConcat->parts = NULL;
            continue;
        }

        if (strncmp(lineBuffer, "parts=", 6) == 0 && currentConcat) {
            int count = atoi(lineBuffer + 6);
            if (count > 0) {
                currentConcat->parts = calloc(count, sizeof(char *));
                currentConcat->partCount = count;
            }
            continue;
        }

        if (strncmp(lineBuffer, "part_", 5) == 0 && currentConcat) {
            int index = atoi(lineBuffer + 5);
            char *eq = strchr(lineBuffer, '=');
            if (eq && index >= 0 && index < currentConcat->partCount) {
                currentConcat->parts[index] = strdup(eq + 1);
            }
            continue;
        }

        // --- STDERR SECTION ---
        if (strncmp(lineBuffer, "stderr=", 7) == 0) {
            if (*stderrs == NULL) {
                stderrCap = 1;
                *stderrs = malloc(stderrCap * sizeof(stderrDef));
                if (!*stderrs) {
                    perror("malloc failed for stderrs");
                    fclose(fp);
                    freeMem(*nodes, *nodeCount, *pipes, *pipeCount, *concats, *concatCount, *stderrs, *stderrCount, *files, *fileCount);
                    exit(1);
                }
            } 
            else if (*stderrCount >= stderrCap) {
                stderrCap *= 2;
                stderrDef *tmp = realloc(*stderrs, stderrCap * sizeof(stderrDef));
                if (!tmp) {
                    perror("realloc failed for stderrs");
                    fclose(fp);
                    freeMem(*nodes, *nodeCount, *pipes, *pipeCount, *concats, *concatCount, *stderrs, *stderrCount, *files, *fileCount);
                    exit(1);
                }
                *stderrs = tmp;
            }

            currentStderr = &(*stderrs)[(*stderrCount)++];
            currentStderr->name = strdup(lineBuffer + 7);
            currentStderr->from = NULL;
            continue;
        }

        if (strncmp(lineBuffer, "from=", 5) == 0 && currentStderr) {
            currentStderr->from = strdup(lineBuffer + 5);
            continue;
        }

        // --- FILE SECTION ---
        if (strncmp(lineBuffer, "file=", 5) == 0) {
            if (*files == NULL) {
                fileCap = 1;
                *files = malloc(fileCap * sizeof(fileDef));
                if (!*files) {
                    perror("malloc failed for files");
                    fclose(fp);
                    freeMem(*nodes, *nodeCount, *pipes, *pipeCount, *concats, *concatCount, *stderrs, *stderrCount, *files, *fileCount);
                    exit(1);
                }
            } 
            else if (*fileCount >= fileCap) {
                fileCap *= 2;
                fileDef *tmp = realloc(*files, fileCap * sizeof(fileDef));
                if (!tmp) {
                    perror("realloc failed for files");
                    fclose(fp);
                    freeMem(*nodes, *nodeCount, *pipes, *pipeCount, *concats, *concatCount, *stderrs, *stderrCount, *files, *fileCount);
                    exit(1);
                }
                *files = tmp;
            }

            currentFile = &(*files)[(*fileCount)++];
            currentFile->name = strdup(lineBuffer + 5);
            currentFile->fileName = NULL;
            continue;
        }
        if (strncmp(lineBuffer, "name=", 5) == 0 && currentFile) {
            currentFile->fileName = strdup(lineBuffer + 5);
            continue;
        }
    }

    fclose(fp);
}

char **splitCommand(const char *command) {
    if (!command) 
        return NULL;

    char *cmdCopy = strdup(command);
    if (!cmdCopy) 
        return NULL;

    char *token;
    char **args = malloc(64 * sizeof(char *));
    if (!args) { 
        free(cmdCopy); 
        return NULL; 
    }

    int position = 0;

    token = strtok(cmdCopy, " ");
    while (token != NULL) {
        if (position >= 63) 
            break; // avoid overflow, keep room for NULL
        // Remove surrounding quotes if any
        int tlen = strlen(token);
        if (tlen >= 2 && ((token[0] == '\'' && token[tlen - 1] == '\'') || (token[0] == '"'  && token[tlen - 1] == '"'))) {
            token[tlen - 1] = '\0';
            token++;
        }
        args[position] = strdup(token);  // deep copy
        if (!args[position]) 
            break;
        position++;
        token = strtok(NULL, " ");
    }
    args[position] = NULL;
    free(cmdCopy);
    return args;
}

int directivePresent (const char *directive, nodeDef *nodes, int nodeCount, pipeDef *pipes, int pipeCount, concatDef *concats, int concatCount, stderrDef *stderrs, int stderrCount, fileDef *files, int fileCount) {
     // --- Check all node blocks ---
    for (int i = 0; i < nodeCount; i++) {
        if (strcmp(nodes[i].name, directive) == 0)
            return 1;
    }

    // --- Check all pipe blocks ---
    for (int i = 0; i < pipeCount; i++) {
        if (strcmp(pipes[i].name, directive) == 0)
            return 1;
    }

    // --- Check all concat blocks ---
    for (int i = 0; i < concatCount; i++) {
        if (strcmp(concats[i].name, directive) == 0)
            return 1;
    }

    // --- Check all stderr blocks ---
    for (int i = 0; i < stderrCount; i++) {
        if (strcmp(stderrs[i].name, directive) == 0)
            return 1;
    }

    // --- Check all file blocks ---
    for (int i = 0; i < fileCount; i++) {
        if (strcmp(files[i].name, directive) == 0)
            return 1;
    }

    // --- Not found anywhere ---
    return 0;
}

void freeArgs(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }
    free(args);
}

void executeFlow(const char *blockName, nodeDef *nodes, int nodeCount, pipeDef *pipes, int pipeCount, concatDef *concats, int concatCount, stderrDef *stderrs, int stderrCount, fileDef *files, int fileCount) { 
    
    static int flowDepth = 0;  
    static int forkCount = 0;
    flowDepth++;

    if (flowDepth > MAX_FLOW_DEPTH) {
        fprintf(stderr, "Error: Cyclical Dependency\n");
        _exit(1);  // Exit immediately in child
    }

    // --- BASE CASE: Check if it's a NODE ---
    for (int i = 0; i < nodeCount; i++) {
        if (strcmp(nodes[i].name, blockName) == 0) {
            
            if (++forkCount > MAX_FORK_LIMIT) {
                fprintf(stderr, "Error: Fork limit exceeded (possible cyclical dependency)\n");
                _exit(1);
            }

            pid_t pid = fork();
            if (pid == 0) {
                char **args = splitCommand(nodes[i].command);
                execvp(args[0], args);
                perror("execvp failed\n");
                freeArgs(args);
                _exit(1);   
            }    
            else if (pid > 0) {
                wait(NULL);
            }
            else {
                perror("fork failed\n");
                freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount); 
                _exit(1);
            }
            flowDepth--;
            return;
        }
    }

    // --- PIPE CASE ---
    for (int i = 0; i < pipeCount; i++) {
        if (strcmp(pipes[i].name, blockName) == 0) {

            int fd[2];
            if (pipe(fd) < 0) {
                perror("pipe failed\n");
                freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount); 
                exit(1);
            }

            if (++forkCount > MAX_FORK_LIMIT) {
                fprintf(stderr, "Error: Fork limit exceeded (possible cyclical dependecy)\n");
                _exit(1);
            }


            pid_t pid = fork();
            if (pid < 0) {
                perror("fork failed\n");
                freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount); 
                _exit(1);
            }

            if (pid == 0) {
                // --- CHILD PROCESS: executes the 'from' side ---
                close(fd[0]);            // Close read end
                dup2(fd[1], STDOUT_FILENO); // Redirect stdout to pipe write end
                close(fd[1]);
                
                // Recursively execute whatever "from" points to
                executeFlow(pipes[i].from, nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount);
                _exit(0);
            } 
            else if (pid > 0) {
                // --- PARENT PROCESS: executes the 'to' side ---
                close(fd[1]);            // Close write end
                dup2(fd[0], STDIN_FILENO); // Redirect stdin to pipe read end
                close(fd[0]);

                // Recursively execute whatever "to" points to
                executeFlow(pipes[i].to, nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount);
                wait(NULL);
                flowDepth--;
                return;
            }
        }
    }
    
    // --- CONCAT CASE ---
    for (int i = 0; i < concatCount; i++) {
        if (strcmp(concats[i].name, blockName) == 0) {
            for (int j = 0; j < concats[i].partCount; j++) {
                // Execute each part sequentially
                executeFlow(concats[i].parts[j], nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount);
            }
            // Return to prevent falling through to other block types
            flowDepth--;
            return;
        }
    }

    // --- STDERR CASE ---
    for (int i = 0; i < stderrCount; i++) {
        if (strcmp(stderrs[i].name, blockName) == 0) {

        if (++forkCount > MAX_FORK_LIMIT) {
                fprintf(stderr, "Error: Fork limit exceeded (possible cyclical dependency)\n");
                _exit(1);
            }
        
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed for stderr block");
            freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount); 
            _exit(1);
        }

        if (pid == 0) {
            // --- CHILD PROCESS: redirect stderr → stdout ---
            dup2(STDOUT_FILENO, STDERR_FILENO);

            // Execute the node whose stderr we’re merging
            executeFlow(stderrs[i].from, nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount);
            _exit(0);
        } 
        else if (pid > 0) {
            // --- PARENT PROCESS ---
            wait(NULL);
            flowDepth--;
            return;
            }
        }
    }

    // --- FILE CASE ---
    for (int i = 0; i < fileCount; i++) {
        if (strcmp(files[i].name, blockName) == 0) {

            if (!files[i].fileName) {
                fprintf(stderr, "Error: file '%s' missing name attribute\n", files[i].name);
                freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount); 
                _exit(1);
            }

            int isInput = 0;
            int isOutput = 0;

            for (int j = 0; j < pipeCount; j++) {
                if (pipes[j].from && strcmp(pipes[j].from, blockName) == 0) {
                    isInput = 1;
                    break;
                }
                if (pipes[j].to && strcmp(pipes[j].to, blockName) == 0) {
                    isOutput = 1;
                    break;
                }
            }

            if (isInput) {
                // --- Input file case ---
                FILE *input = fopen(files[i].fileName, "r");
                if (!input) {
                    perror("Error opening input file");
                    freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount);
                    _exit(1);
                }

                char buffer[1024];
                int n;
                while ((n = fread(buffer, 1, sizeof(buffer), input)) > 0) {
                    if (fwrite(buffer, 1, n, stdout) != n) {
                        perror("write to pipe failed");
                        fclose(input);
                        flowDepth--;
                        freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount);
                        _exit(1);
                    }
                }

                if (ferror(input)) {
                    perror("Error reading input file");
                    fclose(input);
                    flowDepth--;
                    freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount);
                    _exit(1);
                }

                fclose(input);

                if (fflush(stdout) == EOF) {
                    perror("fflush failed");
                    flowDepth--;
                    freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount);
                    _exit(1);
                }

                flowDepth--;
                return;
            }
            else if (isOutput) {
                // --- Output file case ---
                FILE *output = fopen(files[i].fileName, "w");
                if (!output) {
                    perror("Error opening output file");
                    freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount);
                    _exit(1);
                }

                char buffer[1024];
                int n;
                while ((n = fread(buffer, 1, sizeof(buffer), stdin)) > 0) {
                    if (fwrite(buffer, 1, n, output) != n) {
                        perror("Error writing to output file");
                        fclose(output);
                        flowDepth--;
                        freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount);
                        _exit(1);
                    }
                }
                fclose(output);
                return;
            }
        }
    }
}

int hasCycleUtil(const char *block, nodeDef *nodes, int nodeCount, pipeDef *pipes, int pipeCount, concatDef *concats, int concatCount, stderrDef *stderrs, int stderrCount, fileDef *files, int fileCount, char **visited, char **recStack, int depth) {
    visited[depth] = strdup(block);
    recStack[depth] = strdup(block);

    int hasOutgoing = 0;

    // --- PIPE CONNECTIONS ---
    for (int i = 0; i < pipeCount; i++) {
        if (strcmp(pipes[i].from, block) == 0) {
            hasOutgoing = 1;
            char *next = pipes[i].to;

            for (int j = 0; j <= depth; j++)
                if (recStack[j] && strcmp(recStack[j], next) == 0)
                    return 1;

            if (hasCycleUtil(next, nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount, visited, recStack, depth + 1))
                return 1;
        }
    }

    if (!hasOutgoing) {
        int isNodeorFile = 0;
        for (int i = 0; i < nodeCount; i++) {
            if (strcmp(nodes[i].name, block) == 0) {
                isNodeorFile = 1;
                break;
            }
        }
        for (int i = 0; i < fileCount; i++) {
            if (strcmp(files[i].name, block) == 0) {
                isNodeorFile = 1;
                break;
            }
        }
        if (!isNodeorFile) {
            return 1;
        }
    }
    free(recStack[depth]);
    recStack[depth] = NULL;

    return 0;
}

int detectCycles(nodeDef *nodes, int nodeCount, pipeDef *pipes, int pipeCount, concatDef *concats, int concatCount, stderrDef *stderrs, int stderrCount, fileDef *files, int fileCount) {
    char *visited[256] = {0};
    char *recStack[256] = {0};

    for (int i = 0; i < pipeCount; i++) {
        if (hasCycleUtil(pipes[i].from, nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount, visited, recStack, 0)) {
            for (int j = 0; j < 256; j++) {
                if (visited[j]) 
                    free(visited[j]);
                if (recStack[j]) 
                    free(recStack[j]);
            }
            return 1;
        }
    }

    // Clean up
    for (int i = 0; i < 256; i++) {
        if (visited[i]) 
            free(visited[i]);
        if (recStack[i]) 
            free(recStack[i]);
    }

    return 0;
}

