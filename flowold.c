#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    char name[64];
    char command[256];
} nodeDef;

typedef struct {
    char name[64];
    char from[64];
    char to[64];
} pipeDef;

typedef struct {
    char name[64];
    int partCount;
    char parts[16][64];
} concatDef;

typedef struct {
    char name[64];
    char from[64];
} stdErrDef;

typedef struct {
    char file[64];
    char name[64];
} fileDef;

void parse_flow_file(const char *filename, nodeDef *nodes, int *num_nodes, pipeDef *pipes, int *num_pipes, concatDef *concats, int *num_concats) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening flow file");
        exit(1);
    }

    char line[256];
    nodeDef *current_node = NULL;
    pipeDef *current_pipe = NULL;
    concatDef *current_concat = NULL;

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;

        if (strlen(line) == 0) continue;

        if (strncmp(line, "node=", 5) == 0) {
            current_node = &nodes[*num_nodes];
            strcpy(current_node->name, line + 5);
            (*num_nodes)++;
            current_pipe = NULL;
            current_concat = NULL;
        }

        else if (strncmp(line, "command=", 8) == 0 && current_node) {
            strcpy(current_node->command, line + 8);
        }

        else if (strncmp(line, "pipe=", 5) == 0) {
            current_pipe = &pipes[*num_pipes];
            strcpy(current_pipe->name, line + 5);
            (*num_pipes)++;
            current_node = NULL;
            current_concat = NULL;
        }

        else if (strncmp(line, "from=", 5) == 0 && current_pipe) {
            strcpy(current_pipe->from, line + 5);
        } 
        else if (strncmp(line, "to=", 3) == 0 && current_pipe) {
            strcpy(current_pipe->to, line + 3);
        }

        else if (strncmp(line, "concatenate=", 12) == 0){
            current_concat = &concats[*num_concats];
            strcpy(current_concat->name, line + 12);
            (*num_concats)++;
            current_node = NULL;
            current_pipe = NULL;
        }

        else if (strncmp(line, "parts=", 6) == 0 && current_concat) {
            current_concat->partCount = atoi(line + 6);
        }

        // Handles part_0=..., part_1=..., etc.
        else if (strncmp(line, "part_", 5) == 0 && current_concat) {
            int index = atoi(line + 5);
            char *eq = strchr(line, '=');
            if (eq && index >= 0 && index < 16) {
                strcpy(current_concat->parts[index], eq + 1);
            }
        }
    }

    fclose(fp);
}

nodeDef *find_node(nodeDef *nodes, int num_nodes, const char *name) {
    for (int i = 0; i < num_nodes; i++) {
        if (strcmp(nodes[i].name, name) == 0)
            return &nodes[i];
    }
    return NULL;
}

pipeDef *find_pipe(pipeDef *pipes, int num_pipes, const char *name) {
    for (int i = 0; i < num_pipes; i++) {
        if (strcmp(pipes[i].name, name) == 0)
            return &pipes[i];
    }
    return NULL;
}

concatDef *find_concat(concatDef *concats, int num_concats, const char *name) {
    for (int i = 0; i < num_concats; i++) {
        if (strcmp(concats[i].name, name) == 0)
            return &concats[i];
    }
    return NULL;
}

void execute_concat(nodeDef *nodes, int num_nodes, pipeDef *pipes, int num_pipes, concatDef *concats, int num_concats, concatDef *c);

void execute_pipe(nodeDef *nodes, int num_nodes, pipeDef *pipes, int num_pipes, concatDef *concats, int num_concats, pipeDef *p) {
    nodeDef *from_node = find_node(nodes, num_nodes, p->from);
    nodeDef *to_node = find_node(nodes, num_nodes, p->to);
    concatDef *from_concat = find_concat(concats, num_concats, p->from);
    concatDef *to_concat = find_concat(concats, num_concats, p->to);

    if ((!from_node && !from_concat) || (!to_node && !to_concat)) {
        fprintf(stderr, "Error: Could not find source or destination node\n");
        exit(1);
    }

    int fds[2];
    if (pipe(fds) == -1) {
        perror("pipe failed");
        exit(1);
    }

    pid_t pid1 = fork();
    if (pid1 == 0) {
        // ---- CHILD 1 ---- (producer)
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);

        if (from_node) {
            char *args[16];
            int i = 0;
            char *token = strtok(from_node->command, " ");
            while (token && i < 15) {
                args[i++] = token;
                token = strtok(NULL, " ");
            }
            args[i] = NULL;
            execvp(args[0], args);
            perror("execvp failed");
        } 
        else if (from_concat) {
            execute_concat(nodes, num_nodes, pipes, num_pipes, concats, num_concats, from_concat);
        }
        exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        // ---- CHILD 2 ---- (consumer)
        dup2(fds[0], STDIN_FILENO);
        close(fds[1]);
        close(fds[0]);

        if (to_node) {
            char *args[16];
            int i = 0;
            char *token = strtok(to_node->command, " ");
            while (token && i < 15) {
                args[i++] = token;
                token = strtok(NULL, " ");
            }
            args[i] = NULL;
            execvp(args[0], args);
            perror("execvp failed");
        } 
        else if (to_concat) {
            execute_concat(nodes, num_nodes, pipes, num_pipes, concats, num_concats, to_concat);
        }
        exit(1);
    }

    close(fds[0]);
    close(fds[1]);
    wait(NULL);
    wait(NULL);
}


void execute_concat(nodeDef *nodes, int num_nodes, pipeDef *pipes, int num_pipes, concatDef *concats, int num_concats, concatDef *c) {

    for (int i = 0; i < c->partCount; i++) {
        char *part_name = c->parts[i];

        nodeDef *n = find_node(nodes, num_nodes, part_name);
        if (n) {
            printf("Running node: %s -> %s\n", n->name, n->command);

            pid_t pid = fork();
            if (pid == 0) {
                char *args[16];
                int j = 0;
                char *token = strtok(n->command, " ");
                while (token && j < 15) {
                    args[j++] = token;
                    token = strtok(NULL, " ");
                }
                args[j] = NULL;

                execvp(args[0], args);
                perror("execvp failed");
                exit(1);
            }
            wait(NULL);
            continue;
        }

        pipeDef *p = find_pipe(pipes, num_pipes, part_name);
        if (p) {
            printf("Running pipe: %s (%s -> %s)\n", p->name, p->from, p->to);
            execute_pipe(nodes, num_nodes, pipes, num_pipes, concats, num_concats, p);
            continue;
        }

        concatDef *sub = find_concat(concats, num_concats, part_name);
        if (sub) {
            printf("Running concatenate: %s\n", sub->name);
            execute_concat(nodes, num_nodes, pipes, num_pipes, concats, num_concats, sub);
            continue;
        }

        fprintf(stderr, "Error: Unknown part '%s' in concatenate '%s'\n", part_name, c->name);
        exit(1);
    }
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <flowfile> <target>\n", argv[0]);
        return 1;
    }

    nodeDef nodes[50];
    pipeDef pipes[50];
    concatDef concats[50];
    int num_nodes = 0, num_pipes = 0, num_concats = 0;

    parse_flow_file(argv[1], nodes, &num_nodes, pipes, &num_pipes, concats, &num_concats);

    char *target = argv[2];

    pipeDef *p = find_pipe(pipes, num_pipes, target);
    concatDef *c = find_concat(concats, num_concats, target);

    if (p) 
        execute_pipe(nodes, num_nodes, pipes, num_pipes, concats, num_concats, p);
    
    else if (c) 
        execute_concat(nodes, num_nodes, pipes, num_pipes, concats, num_concats, c);
 
    else {
        fprintf(stderr, "Error: Target '%s' not found as pipe or concatenate\n", target);
        return 1;
    }

    return 0;
}


// parse the file to find the nodes and operations
// bottom up approach
// get last part (doit) and work to start
// create a pipe, gives fd's for read and write
// fork to create the source process, in the child call dup2() (redirectis input and output, copies file descriptors)
// once you create your fork, call exec to replace that process (replace ls with fork using)


