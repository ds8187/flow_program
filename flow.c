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
    int *partCount;
    char **parts;
} concatDef;

typedef struct {
    char *name;
    int *partCount;
    char **parts;
} concatDef;

typedef struct {
    char *name;
    char *from;
} stderrDef;

typedef struct {
    char *name;
    int *fileName;
} fileDef;

void parseFlowFile(const char *filename, nodeDef **nodes, int *nodeCount, pipeDef **pipes, int *pipeCount) {
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
            currentPipe = NULL;
        }

        // --- COMMAND SECTION ---
        else if (strncmp(lineBuffer, "command=", 8) == 0 && currentNode) {
            currentNode->command = malloc(strlen(lineBuffer + 8) + 1);
            strcpy(currentNode->command, lineBuffer + 8);
        }

        // --- PIPE SECTION ---
        else if (strncmp(lineBuffer, "pipe=", 5) == 0) {
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
            currentNode = NULL;
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

void freeMem(nodeDef *nodes, int nodeCount, pipeDef *pipes, int pipeCount) {
    for (int i = 0; i < nodeCount; i++) {
        free(nodes[i].name);
        free(nodes[i].command);
    }

    for (int i = 0; i < pipeCount; i++) {
        free(pipes[i].name);
        free(pipes[i].from);
        free(pipes[i].to);
    }

    free(nodes);
    free(pipes);
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <flowfile>\n", argv[0]);
        return 1;
    }

    nodeDef *nodes = NULL;
    pipeDef *pipes = NULL;
    concatDef *concats = NULL;
    stderrDef *stderrs = NULL;
    fileDef *files = NULL;

    int nodeCount = 0, pipeCount = 0;

    parseFlowFile(argv[1], &nodes, &nodeCount, &pipes, &pipeCount);

    printf("=== Parsed Nodes ===\n");
    for (int i = 0; i < nodeCount; i++) {
        printf("Node %d:\n", i + 1);
        printf("  Name: %s\n", nodes[i].name);
        printf("  Command: %s\n", nodes[i].command ? nodes[i].command : "(none)");
    }

    printf("=== Parsed Pipes ===\n");
    for (int i = 0; i < pipeCount; i++) {
        printf("Pipe %d:\n", i + 1);
        printf("  Name: %s\n", pipes[i].name);
        printf("  From: %s\n", pipes[i].from ? pipes[i].from : "(none)");
        printf("  To: %s\n", pipes[i].to ? pipes[i].to : "(none)");
    }

    freeMem(nodes, nodeCount, pipes, pipeCount);
    return 0;
}



