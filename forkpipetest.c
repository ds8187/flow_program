// pipe_example.c
// Creates a pipe:   (cat foo.txt | sed 's/o/u/g')  -->  wc
// Outer pipe: parent forks a reader that runs "wc" reading from outer pipe.
// Writer child creates an inner pipe and runs "cat foo.txt" -> "sed 's/o/u/g'"
// and sends sed's output into the outer pipe.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(void) {
    int outer_pipe[2];

    if (pipe(outer_pipe) == -1) {
        die("pipe(outer)");
    }

    pid_t pid_reader = fork();
    if (pid_reader < 0) {
        die("fork reader");
    }

    if (pid_reader == 0) {
        // Reader child: connect read end of outer_pipe to stdin, exec wc
        // Close unused write end
        close(outer_pipe[1]);

        if (dup2(outer_pipe[0], STDIN_FILENO) == -1) {
            die("dup2 reader");
        }
        close(outer_pipe[0]);

        // Exec wc (reads from stdin)
        execlp("wc", "wc", (char *)NULL);
        // If execlp returns, it's an error:
        fprintf(stderr, "exec wc failed: %s\n", strerror(errno));
        _exit(127);
    }

    // Parent continues: create writer child which will build the inner pipeline
    pid_t pid_writer = fork();
    if (pid_writer < 0) {
        die("fork writer");
    }

    if (pid_writer == 0) {
        // Writer child:
        // Will run: cat foo.txt | sed 's/o/u/g'  and send sed output to outer_pipe[1]
        // Close unused read end of outer pipe
        close(outer_pipe[0]);

        int inner_pipe[2];
        if (pipe(inner_pipe) == -1) {
            die("pipe(inner)");
        }

        // Fork grandchild for 'cat foo.txt' -> writes to inner_pipe[1]
        pid_t pid_cat = fork();
        if (pid_cat < 0) {
            die("fork cat");
        }

        if (pid_cat == 0) {
            // cat child: redirect stdout to inner_pipe[1]
            close(inner_pipe[0]); // close unused read end
            if (dup2(inner_pipe[1], STDOUT_FILENO) == -1) {
                die("dup2 cat");
            }
            close(inner_pipe[1]);

            // Close outer pipe ends (not used)
            close(outer_pipe[1]);

            // exec cat foo.txt
            execlp("cat", "cat", "foo.txt", (char *)NULL);
            fprintf(stderr, "exec cat failed: %s\n", strerror(errno));
            _exit(127);
        }

        // Fork grandchild for 'sed' -> reads from inner_pipe[0], writes to outer_pipe[1]
        pid_t pid_sed = fork();
        if (pid_sed < 0) {
            die("fork sed");
        }

        if (pid_sed == 0) {
            // sed child: read from inner_pipe[0], write to outer_pipe[1]
            close(inner_pipe[1]); // close unused write end of inner
            if (dup2(inner_pipe[0], STDIN_FILENO) == -1) {
                die("dup2 sed stdin");
            }
            close(inner_pipe[0]);

            // dup outer_pipe[1] to stdout
            if (dup2(outer_pipe[1], STDOUT_FILENO) == -1) {
                die("dup2 sed stdout");
            }
            close(outer_pipe[1]);

            // exec sed 's/o/u/g'
            // sed accepts script as first non-option argument
            execlp("sed", "sed", "s/o/u/g", (char *)NULL);
            fprintf(stderr, "exec sed failed: %s\n", strerror(errno));
            _exit(127);
        }

        // Writer parent: we are the writer process.
        // Close pipe endpoints we don't need here and wait for grandchildren.
        close(inner_pipe[0]);
        close(inner_pipe[1]);

        // Leave outer_pipe[1] open until children finish sending data.
        close(outer_pipe[1]); // once grandchildren run, writer can close its copy
                              // (sed has its own dup'd fd to outer_pipe[1])

        // Wait for the grandchild processes to finish
        int status;
        waitpid(pid_cat, &status, 0);
        waitpid(pid_sed, &status, 0);

        _exit(EXIT_SUCCESS);
    }

    // Parent: close both ends of outer pipe in parent side except read is used by reader child.
    // Parent already has no role in data movement; close both ends here.
    close(outer_pipe[0]);
    close(outer_pipe[1]);

    // Wait for both reader and writer children
    int status;
    waitpid(pid_writer, &status, 0);
    waitpid(pid_reader, &status, 0);

    return 0;
}
