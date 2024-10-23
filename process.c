#include "process.h"


// Function to set local environment variables
void set_local_environment_variables(const CMD *cmd) {
    for (int i = 0; i < cmd->nLocal; i++) {
        setenv(cmd->locVar[i], cmd->locVal[i], 1);
    }
}

void redirection(const CMD *cmd) {
    // Handle input redirection
    if (cmd->fromType == RED_IN) {
        int fd = open(cmd->fromFile, O_RDONLY);
        if (fd == -1) {
            int error = errno;
            perror("Bad Fork");
            exit(error);
        }

        if (dup2(fd, STDIN_FILENO) == -1) {
            int error = errno;
            perror("Bad Fork");
            exit(error);
        }
        close(fd);
    } else if (cmd->fromType == RED_IN_HERE) {
        // Create a temporary file
        char template[] = "/tmp/XXXXXX";
        int fd_temp = mkstemp(template);

        if (fd_temp == -1) {
            int error = errno;
            perror("Bad Fork");
            exit(error);
        }

        // Write the contents of the here document to the temporary file
        if (write(fd_temp, cmd->fromFile, strlen(cmd->fromFile)) == -1) {
            int error = errno;
            perror("Bad Fork");
            exit(error);
        }
        close(fd_temp);

        // Open the temporary file for reading
        int fd = open(template, O_RDONLY);
        if (fd == -1) {
            int error = errno;
            perror("Bad Fork");
            exit(error);
        }

        // Redirect stdin
        if (dup2(fd, STDIN_FILENO) == -1) {
            int error = errno;
            perror("Bad Fork");
            exit(error);
        }
        close(fd);
    }

    // Handle output redirection
    if (cmd->toType == RED_OUT) {
        int fd = open(cmd->toFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            int error = errno;
            perror("Bad Fork");
            exit(error);
        }
        if (dup2(fd, STDOUT_FILENO) == -1) {
            int error = errno;
            perror("Bad Fork");
            exit(error);
        }
        close(fd);
    } else if (cmd->toType == RED_OUT_APP) {
        int fd = open(cmd->toFile, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd == -1) {
            int error = errno;
            perror("Bad Fork");
            exit(error);
        }
        if (dup2(fd, STDOUT_FILENO) == -1) {
            int error = errno;
            perror("Bad Fork");
            exit(error);
        }
        close(fd);
    }

}


int change_directory(const CMD *cmd) {
    if (cmd->argc == 1 || strcmp(cmd->argv[1], "~") == 0) {
        const char *home = getenv("HOME");
        if (home == NULL) {
            perror("");
            return 1;
            //exit(EXIT_FAILURE);
        }

        if (chdir(home) != 0) {
            perror("chdir");
            return 1;
            //exit(EXIT_FAILURE);
        }

        return 0;
    } else if (cmd->argc > 2)
    {
        perror("argc");
        return 1;
    }else
        {
        if (chdir(cmd->argv[1]) != 0) {
            perror("chdir");
            return 1;
            //exit(EXIT_FAILURE);
        }
        return 0;
    }
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Define a structure for stack nodes
typedef struct Node {
    char directory[256]; 
    struct Node *next;
} Node;

Node *top = NULL; 

void push(const char *directory) {
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL) {
        perror("Memory error");
        exit(EXIT_FAILURE);
    }
    //printf("Current Directory: %s", directory);
    strcpy(newNode->directory, directory);
    newNode->next = top;
    top = newNode;
}

void pop() 
{
    if (top == NULL) {
        exit(EXIT_FAILURE);
    }
    Node *temp = top;
    top = top->next;
    free(temp);
}

void printStack() {
    Node *current = top;
    while (current != NULL) {
        printf(" %s", current->directory);
        current = current->next;
    }
    printf("\n");
}

void pushd(const char *dirName) {
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        return;
    }
    push(cwd); 
    if (chdir(dirName) != 0) {
        perror("chdir");
        pop(); 
        return;
    }
    printf("%s", getcwd(NULL, 0));
    printStack(); 
}

void popd() {
    if (top == NULL) {
        return;
    }
    printStack(); 
    char dir[256];
    strcpy(dir, top->directory);
    pop(); 
    if (chdir(dir) != 0) {
        perror("chdir");
        return;
    }
}

// helper function takes command list
// takes a boolean that should be backgrounded or not
// left is always background 
// boolean keep track if right is background
// call background on node and background node -- call handle background node - and t

// left sep and sep bg -- handler and backgroun fork print have bg and another bg then know left and right needs to be bg--
int simpleprocess(const CMD *cmd, bool flag) {
    if (strcmp("pushd", cmd->argv[0]) == 0) {
        if (cmd->argc != 2) {
            return 1;
        }
        pushd(cmd->argv[1]);
        return 0;
    } else if (strcmp("popd", cmd->argv[0]) == 0) {
        if (cmd->argc != 1) {
            
            return 1;
        }
        popd();
        return 0;
    }
    else if (strcmp("cd", cmd->argv[0]) == 0) {
        return change_directory(cmd);
    } 
    else 
    {
        // Fork a child process
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            for (int i = 0; i < cmd->nLocal; i++) {
                setenv(cmd->locVar[i], cmd->locVal[i], 1);
            }
            
            redirection(cmd);

            execvp(cmd->argv[0], cmd->argv);

            perror("execvp");
            exit(0); 
        } else {
            int status;
            if (flag)
            {
                fprintf(stderr, "Backgrounded: %d\n", pid);
                return 0;
            } else 
            {
                waitpid(pid, &status, 0);

                //return STATUS(status);
                // Set the exit status
                if (WIFEXITED(status)) {
                    setenv("?", WEXITSTATUS(status) == 0 ? "0" : "1", 1);
                }
                return STATUS(status);
            }
        }
    }
}

int andorHelper(const CMD *cmdList, int a, bool flag) {
    int status = process(cmdList->left);
    // this is for the and
    if (a == 0) {
        if (status != 0) {
            return STATUS(status);
        } else {
            return process(cmdList->right);
        }
    // This is for the or
    } else {
        if (status == 0) {
            return 0;
        } else {
            return process(cmdList->right);
        }
    }
}

int pipeprocess(const CMD *cmdList, bool flag)
{
    int pipe_fd[2]; 

    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t left_pid = fork();
    if (left_pid == -1) {
        // Error occurred
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (left_pid == 0) {

        dup2(pipe_fd[1], 1);
        close(pipe_fd[1]); 
        close(pipe_fd[0]);

        exit(process(cmdList->left));
    }

    pid_t right_pid = fork();
    if (right_pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (right_pid == 0) {

        dup2(pipe_fd[0], 0);
        close(pipe_fd[0]); 
        close(pipe_fd[1]);

        exit(process(cmdList->right));
    }


    close(pipe_fd[0]);
    close(pipe_fd[1]);
    int left_status, right_status;
    waitpid(left_pid, &left_status, 0); 
    waitpid(right_pid, &right_status, 0);

    if (STATUS(right_status))
    {
        return setenv("?", WEXITSTATUS(right_status) == 0 ? "0": "1", 1);
    }
    else
    {
        return setenv("?", WEXITSTATUS(left_status) == 0 ? "0": "1", 1);
    }
}
// take boolean to bg
void sep_end_helper(const CMD *cmdList, bool flag)
{
    process(cmdList->left);
    // This will wait for the left to finish processing
    wait(NULL);
    process(cmdList->right);
}

int subcmd_helper(const CMD *cmdList, bool flag) {
    // Fork a child process
    pid_t pid = fork();

    if (pid == -1) {
        int error = errno;
        perror("Bad Fork");
        exit(error);
    } else if (pid == 0) {
        for (int i = 0; i < cmdList->nLocal; i++) {
            setenv(cmdList->locVar[i], cmdList->locVal[i], 1);
        }
        
        redirection(cmdList);
        exit(process(cmdList->left));

    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            setenv("?", WEXITSTATUS(status) == 0 ? "0" : "1", 1);
        }
        return STATUS(status);
    }
}

int process_helper(const CMD *cmdList, bool flag) {
    if (cmdList != NULL)
    {
        switch (cmdList->type) {
        
            case SIMPLE:
                return simpleprocess(cmdList, flag);
                break;
            case SUBCMD:
                return subcmd_helper(cmdList, flag);
                break;

            case SEP_END:
                sep_end_helper(cmdList, flag);
                break;
            
            case PIPE:
                return pipeprocess(cmdList, flag);
                break;

            case SEP_AND:
                return andorHelper(cmdList, 0, flag);
                break;

            case SEP_OR:
                return andorHelper(cmdList, 1, flag);
                break;

            case SEP_BG:
                process_helper(cmdList->left, true);
                process_helper(cmdList->right, flag);
                
                return 0;
                // process_Helper(cmd, flag) flag -- tells if background
                // process left and right and when get to node tell whether to be background
                // process command check if set or not
                break;
            default:
                break;
        }
    }
}


int process(const CMD *cmdList) {
    int status;
    int pid = waitpid(-1, &status, WNOHANG);
    // while loop 
    if (pid > 0)
    {
        fprintf(stderr, "Completed: %d (%d)\n", pid, STATUS(status));
    }
    return process_helper(cmdList, false);
}
