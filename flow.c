#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


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

/*
typedef struct {
    char *name;
    int *fileName;
} fileDef;
 */

void parseFlowNodes(const char *filename, nodeDef **nodes, int *nodeCount) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening flow file");
        exit(1);
    }

    char lineBuffer[512];
    nodeDef *currentNode = NULL;
    pipeDef *currentPipe = NULL;

    while (fgets(lineBuffer, sizeof(lineBuffer), fp)) {
        lineBuffer[strcspn(lineBuffer, "\n")] = '\0';

        if (strlen(lineBuffer) == 0)
            continue;

        // --- NODE SECTION ---
        if (strncmp(lineBuffer, "node=", 5) == 0) {
            *nodes = realloc(*nodes, (*nodeCount + 1) * sizeof(nodeDef));
            if (!*nodes) {
                perror("realloc failed for nodes");
                fclose(fp);
                exit(1);
            }

            currentNode = &(*nodes)[*nodeCount];
            currentNode->name = malloc(strlen(lineBuffer + 5) + 1);
            strcpy(currentNode->name, lineBuffer + 5);
            currentNode->command = NULL;

            (*nodeCount)++;
        }

        // --- COMMAND SECTION ---
        else if (strncmp(lineBuffer, "command=", 8) == 0 && currentNode) {
            currentNode->command = malloc(strlen(lineBuffer + 8) + 1);
            strcpy(currentNode->command, lineBuffer + 8);
        }
    }
    fclose(fp);
}

void parseFlowPipes(const char *filename, pipeDef **pipes, int *pipeCount) {
        FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening flow file");
        exit(1);
    }

    char lineBuffer[512];
    pipeDef *currentPipe = NULL;

    while (fgets(lineBuffer, sizeof(lineBuffer), fp)) {
        lineBuffer[strcspn(lineBuffer, "\n")] = '\0';

        if (strlen(lineBuffer) == 0)
            continue;

        // --- PIPE SECTION ---
        if (strncmp(lineBuffer, "pipe=", 5) == 0) {
            *pipes = realloc(*pipes, (*pipeCount + 1) * sizeof(pipeDef));
            if (!*pipes) {
                perror("realloc failed for pipes");
                fclose(fp);
                exit(1);
            }

            currentPipe = &(*pipes)[*pipeCount];
            currentPipe->name = malloc(strlen(lineBuffer + 5) + 1);
            strcpy(currentPipe->name, lineBuffer + 5);
            currentPipe->from = NULL;
            currentPipe->to = NULL;

            (*pipeCount)++;
        }

        // --- PIPE FROM ---
        else if (strncmp(lineBuffer, "from=", 5) == 0 && currentPipe) {
            currentPipe->from = malloc(strlen(lineBuffer + 5) + 1);
            strcpy(currentPipe->from, lineBuffer + 5);
        }

        // --- PIPE TO ---
        else if (strncmp(lineBuffer, "to=", 3) == 0 && currentPipe) {
            currentPipe->to = malloc(strlen(lineBuffer + 3) + 1);
            strcpy(currentPipe->to, lineBuffer + 3);
        }
    }

    fclose(fp);
}

void parseFlowConcats(const char *filename, concatDef **concats, int *concatCount) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening flow file");
        exit(1);
    }

    char lineBuffer[512];
    concatDef *currentConcat = NULL;

    while (fgets(lineBuffer, sizeof(lineBuffer), fp)) {
        lineBuffer[strcspn(lineBuffer, "\n")] = '\0';
        if (strlen(lineBuffer) == 0)
            continue;

        // --- CONCAT SECTION ---
        if (strncmp(lineBuffer, "concatenate=", 12) == 0) {
            *concats = realloc(*concats, (*concatCount + 1) * sizeof(concatDef));
            if (!*concats) {
                perror("realloc failed for concats");
                fclose(fp);
                exit(1);
            }

            currentConcat = &(*concats)[*concatCount];
            memset(currentConcat, 0, sizeof(concatDef));

            currentConcat->name = malloc(strlen(lineBuffer + 12) + 1);
            strcpy(currentConcat->name, lineBuffer + 12);

            currentConcat->partCount = 0;
            currentConcat->parts = NULL; 

            (*concatCount)++;
        }

        // --- PART COUNT SECTION ---
        else if (strncmp(lineBuffer, "parts=", 6) == 0 && currentConcat) {
            int count = atoi(lineBuffer + 6);
            if (count > 0) {
                currentConcat->parts = malloc(count * sizeof(char *));
                for (int i = 0; i < count; i++) {
                    currentConcat->parts[i] = NULL;
                }
                currentConcat->partCount = count;
            }
        }

        // --- INDIVIDUAL PARTS: part_0=..., part_1=..., etc. ---
        else if (strncmp(lineBuffer, "part_", 5) == 0 && currentConcat) {
            int index = atoi(lineBuffer + 5);
            char *eq = strchr(lineBuffer, '=');
            if (eq && index >= 0 && index < currentConcat->partCount) {
                const char *value = eq + 1;
                currentConcat->parts[index] = malloc(strlen(value) + 1);
                strcpy(currentConcat->parts[index], value);
            }
        }
    }

    fclose(fp);
}

void parseFlowStderr(const char *filename, stderrDef **stderrs, int *stderrCount) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening flow file");
        exit(1);
    }

    char lineBuffer[512];
    stderrDef *currentStderr = NULL;

    while (fgets(lineBuffer, sizeof(lineBuffer), fp)) {
        lineBuffer[strcspn(lineBuffer, "\n")] = '\0';

        if (strlen(lineBuffer) == 0)
            continue;

        // --- STDERR SECTION ---
        if (strncmp(lineBuffer, "stderr=", 7) == 0) {
            *stderrs = realloc(*stderrs, (*stderrCount + 1) * sizeof(stderrDef));
            if (!*stderrs) {
                perror("realloc failed for stderrs");
                fclose(fp);
                exit(1);
            }

            currentStderr = &(*stderrs)[*stderrCount];
            currentStderr->name = malloc(strlen(lineBuffer + 7) + 1);
            strcpy(currentStderr->name, lineBuffer + 7);
            currentStderr->from = NULL;

            (*stderrCount)++;
        }

        // --- FROM SECTION ---
        else if (strncmp(lineBuffer, "from=", 5) == 0 && currentStderr) {
            currentStderr->from = malloc(strlen(lineBuffer + 5) + 1);
            strcpy(currentStderr->from, lineBuffer + 5);
        }   
    }
     fclose(fp);
}

void freeMem(nodeDef *nodes, int nodeCount, pipeDef *pipes, int pipeCount, concatDef *concats, int concatCount, stderrDef *stderrs, int stderrCount) {
    // --- Free nodes ---
    for (int i = 0; i < nodeCount; i++) {
        free(nodes[i].name);
        free(nodes[i].command);
    }
    free(nodes);

    // --- Free pipes ---
    for (int i = 0; i < pipeCount; i++) {
        free(pipes[i].name);
        free(pipes[i].from);
        free(pipes[i].to);
    }
    free(pipes);

    // --- Free concats ---
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

    // --- Free stderr mappings ---
    for (int i = 0; i < stderrCount; i++) {
        free(stderrs[i].name);
        free(stderrs[i].from);
    }
    free(stderrs);

    // --- Free files ---
    /*
    for (int i = 0; i < fileCount; i++) {
        free(files[i].name);
        free(files[i].fileName);
    }
    free(files);
    */
}

char **splitCommand(const char *command) {
    char **args = NULL;
    int count = 0;
    char *cmdCopy = strdup(command);
    char *token = strtok(cmdCopy, " ");
    while (token) {
        args = realloc(args, sizeof(char*) * (count + 1));
        args[count++] = strdup(token);
        token = strtok(NULL, " ");
    }
    args = realloc(args, sizeof(char*) * (count + 1));
    args[count] = NULL;
    free(cmdCopy);
    return args;
}

void executeFlow(const char *blockName, nodeDef *nodes, int nodeCount, pipeDef *pipes, int pipeCount, concatDef *concats, int concatCount, stderrDef *stderrs, int stderrCount) { 
    // 1. --- BASE CASE: Check if it's a NODE ---
    for (int i = 0; i < nodeCount; i++) {
        if (strcmp(nodes[i].name, blockName) == 0) {
            
            pid_t pid = fork();
            if (pid == 0) {
                char **args = splitCommand(nodes[i].command);
                execvp(args[0], args);
                perror("execvp failed");
                exit(1);   
            }    
            else if (pid > 0) {
                wait(NULL);
            }
            else {
                perror("fork failed");
                exit(1);
            }
        }
    }

    // --- 2. PIPE CASE ---
    for (int i = 0; i < pipeCount; i++) {
        if (strcmp(pipes[i].name, blockName) == 0) {

            int fd[2];
            if (pipe(fd) == -1) {
                perror("pipe failed");
                exit(1);
            }

            pid_t pid = fork();
            if (pid == -1) {
                perror("fork failed");
                exit(1);
            }

            if (pid == 0) {
                // --- CHILD PROCESS: executes the 'from' side ---
                close(fd[0]);            // Close read end
                dup2(fd[1], STDOUT_FILENO); // Redirect stdout to pipe write end
                close(fd[1]);
                
                // Recursively execute whatever "from" points to
                executeFlow(pipes[i].from, nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount);
                exit(0);
            } 
            else {
                // --- PARENT PROCESS: executes the 'to' side ---
                close(fd[1]);            // Close write end
                dup2(fd[0], STDIN_FILENO); // Redirect stdin to pipe read end
                close(fd[0]);

                // Recursively execute whatever "to" points to
                executeFlow(pipes[i].to, nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount);
                wait(NULL);
                return;
            }
        }
    }


    

        


    // 3. --- CONCAT CASE ---



}



int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <flowfile>\n", argv[0]);
        return 1;
    }
    
    // --- Allocate and initialize all structures ---
    nodeDef *nodes = NULL;
    pipeDef *pipes = NULL;
    concatDef *concats = NULL;
    stderrDef *stderrs = NULL;
    //fileDef *files = NULL;

    int nodeCount = 0, pipeCount = 0, concatCount = 0, stderrCount = 0; //, fileCount = 0;

    // --- Parse each section individually ---
    parseFlowNodes(argv[1], &nodes, &nodeCount);
    parseFlowPipes(argv[1], &pipes, &pipeCount);
    parseFlowConcats(argv[1], &concats, &concatCount);
    parseFlowStderr(argv[1], &stderrs, &stderrCount);
    // parseFlowFiles(argv[1], &files, &fileCount);  // uncomment if implemented
    /*
    // --- Print out parsed data ---
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

    // --- Cleanup all allocated memory ---


    executeFlow(argv[2], nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount);

    freeMem(nodes, nodeCount, pipes, pipeCount, concats, concatCount, stderrs, stderrCount); //, files, fileCount);

    //printf("\nAll memory freed successfully.\n");
    return 0;
}