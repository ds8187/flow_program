#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

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
char **splitCommand(const char *command);
void freeArgs(char **args);
void executeFlow(const char *blockName, nodeDef *nodes, int nodeCount, pipeDef *pipes, int pipeCount, concatDef *concats, int concatCount, stderrDef *stderrs, int stderrCount, fileDef *files, int fileCount);

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

    // if no nodes, execute flow will enter infinite recursion
    if (nodeCount == 0) {
        return 0;
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
        if (position >= 63) break; // avoid overflow, keep room for NULL
        // Remove surrounding quotes if any
        int tlen = strlen(token);
        if (tlen >= 2 && ((token[0] == '\'' && token[tlen - 1] == '\'') || (token[0] == '"'  && token[tlen - 1] == '"'))) {
            token[tlen - 1] = '\0';
            token++;
        }
        args[position] = strdup(token);  // deep copy
        if (!args[position]) break;
        position++;
        token = strtok(NULL, " ");
    }
    args[position] = NULL;
    free(cmdCopy);
    return args;
}

void freeArgs(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }
    free(args);
}

void executeFlow(const char *blockName, nodeDef *nodes, int nodeCount, pipeDef *pipes, int pipeCount, concatDef *concats, int concatCount, stderrDef *stderrs, int stderrCount, fileDef *files, int fileCount) { 
    // --- BASE CASE: Check if it's a NODE ---
    for (int i = 0; i < nodeCount; i++) {
        if (strcmp(nodes[i].name, blockName) == 0) {
            
            pid_t pid = fork();
            if (pid == 0) {
                char **args = splitCommand(nodes[i].command);
                execvp(args[0], args);
                perror("execvp failed");
                freeArgs(args);
                exit(1);   
            }    
            else if (pid > 0) {
                wait(NULL);
            }
            else {
                perror("fork failed");
                freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount); 
                exit(1);
            }
        }
    }

    // --- PIPE CASE ---
    for (int i = 0; i < pipeCount; i++) {
        if (strcmp(pipes[i].name, blockName) == 0) {

            int fd[2];
            if (pipe(fd) == -1) {
                perror("pipe failed");
                freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount); 
                exit(1);
            }

            pid_t pid = fork();
            if (pid == -1) {
                perror("fork failed");
                freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount); 
                exit(1);
            }

            if (pid == 0) {
                // --- CHILD PROCESS: executes the 'from' side ---
                close(fd[0]);            // Close read end
                dup2(fd[1], STDOUT_FILENO); // Redirect stdout to pipe write end
                close(fd[1]);
                
                // Recursively execute whatever "from" points to
                executeFlow(pipes[i].from, nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount);
                exit(0);
            } 
            else {
                // --- PARENT PROCESS: executes the 'to' side ---
                close(fd[1]);            // Close write end
                dup2(fd[0], STDIN_FILENO); // Redirect stdin to pipe read end
                close(fd[0]);

                // Recursively execute whatever "to" points to
                executeFlow(pipes[i].to, nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount);
                wait(NULL);
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
            return;
        }
    }

    // --- STDERR CASE ---
    for (int i = 0; i < stderrCount; i++) {
        if (strcmp(stderrs[i].name, blockName) == 0) {

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed for stderr block");
            freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount); 
            exit(1);
        }

        if (pid == 0) {
            // --- CHILD PROCESS: redirect stderr → stdout ---
            dup2(STDOUT_FILENO, STDERR_FILENO);

            // Execute the node whose stderr we’re merging
            executeFlow(stderrs[i].from, nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount);
            exit(0);
        } 
        else {
            // --- PARENT PROCESS ---
            wait(NULL);
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
            exit(1);
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
                    exit(1);
                }

                char buffer[1024];
                int n;
                while ((n = fread(buffer, 1, sizeof(buffer), input)) > 0) {
                    if (fwrite(buffer, 1, n, stdout) != n) {
                        perror("write to pipe failed");
                        fclose(input);
                        freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount); 
                        exit(1);
                    }
                }

                fclose(input);
                return;
            }
            else if (isOutput) {
                // --- Output file case ---
                FILE *output = fopen(files[i].fileName, "w");
                if (!output) {
                    perror("Error opening output file");
                    freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount);
                    exit(1);
                }

                char buffer[1024];
                int n;
                while ((n = fread(buffer, 1, sizeof(buffer), stdin)) > 0) {
                    if (fwrite(buffer, 1, n, output) != n) {
                        perror("Error writing to output file");
                        fclose(output);
                        freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount, files, fileCount);
                        exit(1);
                    }
                }
                fclose(output);
                return;
            }
        }
    }
}

/*
 //--- Print out parsed data ---
    printf("\n=== Parsed Nodes ===\n");
    for (int i = 0; i < nodeCount; i++) {
        printf("Node %d:\n", i + 1);
        printf("  Name: %s\n", nodes[i].name);
        printf("  Command: %s\n", nodes[i].command ? nodes[i].command : "(none)");
    }

    printf("\n=== Parsed Pipes ===\n");
    for (int i = 0; i < pipeCount; i++) {
        printf("Pipe %d:\n", i + 1);
        printf("  Name: %s\n", pipes[i].name);
        printf("  From: %s\n", pipes[i].from ? pipes[i].from : "(none)");
        printf("  To: %s\n", pipes[i].to ? pipes[i].to : "(none)");
    }

    printf("\n=== Parsed Concatenations ===\n");
    for (int i = 0; i < concatCount; i++) {
        printf("Concat %d:\n", i + 1);
        printf("  Name: %s\n", concats[i].name);
        printf("  Parts (%d):\n", concats[i].partCount);
        for (int j = 0; j < concats[i].partCount; j++) {
            printf("    Part %d: %s\n", j, concats[i].parts[j]);
        }
    }

    printf("\n=== Parsed Stderr Redirections ===\n");
    for (int i = 0; i < stderrCount; i++) {
        printf("Stderr %d:\n", i + 1);
        printf("  Name: %s\n", stderrs[i].name);
        printf("  From: %s\n", stderrs[i].from ? stderrs[i].from : "(none)");
    }
    
    printf("\n=== Parsed Files ===\n");
    for (int i = 0; i < fileCount; i++) {
        printf("File %d:\n", i + 1);
        printf("  Name: %s\n", files[i].name);
        printf("  FileName: %s\n", files[i].fileName ? files[i].fileName : "(none)");
    }
    */