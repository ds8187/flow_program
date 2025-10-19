Logic Flow of main function:
    - Initialization and input validation. 
    - Make sure that user input includes flow executable, a file, and a directive.
    - Initialize an array of all structures and a count of all structures
    - Execute parseFlowFile which populates all of my arrays and updates the counts
    - Execute directivePresent which check if the directive passed by the user in arrgv[2] is present in the flow file (if not throw an error)
    - Verify that the flow contains at least one node which is the base case for the recursive function executeFlow
    - Execute detectCycles, which uses hasCycleUtil. Traces the recursion particulary from pipes to see if the same directives are visited more than once, if yes there is a cycle dependency and throw error.
    - After these three checks have been succesfully passed run execute flow which recursively executes each directive in the flow path
    - Run freeMem after succesful execution to free any malloc'd memory (directive arrays)

parseFlowFile:
    - The function parseFlowFile() reads the given configuration file line by line.
    - It dynamically allocates memory for each new block type and extracts:
        - Node names and commands (node=, command=)
        - Pipe connections (pipe=, from=, to=)
        - Concatenation lists (concatenate=, parts=, part_#=)
        - Error redirections (stderr=, from=)
        - File definitions (file=, name=)
        - The parser uses progressive malloc/realloc calls with capacity doubling to safely handle an arbitrary number of blocks.
            - realloc also creates a temporary pointer to ensure that an error does not cause a heap memory section to be lost and never freed
        - Invalid lines or allocation failures terminate execution safely with error reporting.

directivePresent:
    - Loop through all directive arrays and compare argv[2]
    - If found return 1, if not return 0

executeFlow:
    - Takes directive arrays and counts as inputs as well as blockName which tracks which block to execute in each call
    - Recursive function
    - Each time it is called increment flowDepth to track recursion depth and make sure it doesn't pass the MAX_FLOW_DEPTH limit
    - Assumptions made for protection: 
        - No reasonably written flow file will require more than 50 forks
        - No reasonably written flow file will require executeFlow to be called over 64 times
        - Both of these are to prevent possible cyclical dependecies, infinite recursion, or fork bombs
    - Node Blocks (base case):
        - When encountering a node block, the program:
        - Forks a child process.
        - Splits the command into arguments via splitCommand()
        - Replaces child process with execvp(), calling the command that was returned in splitCommand
        - call freeArgs to free the dynamically allocated vector in execvp (returned from splitCommand)
        - The parent waits for the child to complete before continuing
        - Two safety mechanisms are enforced:
            - MAX_FLOW_DEPTH prevents infinite recursion (e.g., from cyclic dependencies)
            - decrement flowDepth tracker to make sure recursion limit (MAX_Flow_DEPTH) isn't hit
    - Pipe Blocks:
        - Creates a pipe
        - Forks a child process for the from block (writer side)
        - Redirects its stdout into the pipe
        - In the parent, redirect stdin to the pipe’s read end and executes the to block
        - Also enforces MAX_FORK_LIMIT
        - call executeFlow and recursively execute to block the same way
        - decrement flowDepth tracker
    - Concatenation Blocks:
        - Each concatDef struct contains an array of parts
        - When a concat is passed, execute each part in parts sequentially with a recursive executeFlow call
        - decrement flowDepth tracker
    - StdErr Blocks:
        - increment forkCount and fork to child process
        - in child use dup2 to redirect stderr to stdout
        - executeFlow recursively to execute the node
        - if the node produces an error it will be passed as standard output
        - if not the standard output will still pass
        - parent waits and decrements recursion depth
    - File blocks:
        - define if the file is an input or output
        - throw errors if file cannot be opened and exit
        - input:
            - read each line in the file
            - write it to standard output (if unable to throw error)
            - close file
        - output:
            - read from stdinput
            - write to the file
            - close file
    - all forks, memory allocations, etc. have protections to throw errors if a system call does not work and end the execution of the program

splitCommand and free Args:
    - splitCommand returns a dynamically allocated vector of strings to be used in execvp in executeFlow
    - freeArgs frees that vector after execvp is called

detectCycles:
    - initializes arrays visited and recStack:
        - visited holds all blocks we have come across
        - recstack holds all blocks being executed by the recursion
    - for each pipe call hasCycleUtil
        - if hasCycleUtil returns 1 then free items in arrays and return 1
    - if no error free items in arrays
    - return 0 if no call to hasCycleUtil returned 1

hasCycleUtil:
    - walks the recursion path
    - keeps track of blocks currently in call stack
        - flags a cycle if it sees the same node twice on that path
    - mark current node as visited and on recStack
    - check outgoing pipe connnections
    - if this is a from block, there is an outgoing connection
    - check if the next block (to block) is on the recStack
        - if it is flag cycle and return 1
    - recursively explore next block
    - if there are no outgoing blocks, ensure this is a valid terminla block. a node to execute or file to output to