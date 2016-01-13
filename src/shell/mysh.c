#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>

typedef struct command {
    char ** tokens;
    char * input;
    char * output;
    struct command * next;
} command;

command * new_command() {
    command * cmd = (command *) malloc(sizeof(command));
    cmd->tokens = NULL;
    cmd->input = NULL;
    cmd->output = NULL;
    cmd->next = NULL;
    return cmd;
}

void print_cmd_list(command * cmd_pt) {
    int i = 0;
    int j;
    for (; cmd_pt != NULL; cmd_pt = cmd_pt->next) {
        printf("command %d:\n\
  tokens:\n", i);
        for (j = 0; cmd_pt->tokens[j] != NULL; j++) {
            printf("    %s\n", cmd_pt->tokens[j]);
        }
        printf("  input: %s\n  output: %s\n", cmd_pt->input, cmd_pt->output);
        i++;
    }
}

/*
 * Free memory associated with a linked list of commands.
 */
void cleanup_commands(command * first_command) {
    if (!first_command) {
        return;
    }
    if (first_command->tokens) {
        char ** token_pointer = first_command->tokens;
        while (*token_pointer) {
            free(*token_pointer);
            token_pointer++;
        }
        free(first_command->tokens);
    }
    if (first_command->input) {
        free(first_command->input);
    }
    if (first_command->output) {
        free(first_command->output);
    }
    command * next_command = first_command->next;
    free(first_command);
    cleanup_commands(next_command);
}

/*
 * Attempt to execute the external command represented by cmd->tokens.
 * Process will exit if command succeeds, but not if it fails.
 */
void execute_command(command * cmd) {
    if (execvp(cmd->tokens[0], cmd->tokens) == -1) {
        fprintf(stderr, "mysh: An error occurred while executing command \"%s\"\n", cmd->tokens[0]);
        perror("execve");
    }
}

/*
 * Return true if cmd->tokens represents an internal command.
 */
bool is_internal_command(command * cmd) {
    char * first_token = cmd->tokens[0];
    return (strcmp(first_token, "cd") == 0 || strcmp(first_token, "exit") == 0);
}

/*
 * If cmd->tokens represents an internal command, then execute it, handle any
 * errors, ignore any subsequent commands linked to by cmd->next, and return
 * true. Otherwise, return false.
 */
bool handle_internal_command(command * cmd) {
    char * first_token = cmd->tokens[0];
    if (strcmp(first_token, "exit") == 0) {
        exit(EXIT_SUCCESS);
    } else if (strcmp(first_token, "cd") == 0) {
        // tokens[1] is a valid dereference because tokens is always terminated
        // by NULL
        char * to_dir = cmd->tokens[1];
        if (to_dir == NULL) {
            to_dir = getenv("HOME");
        }
        if (chdir(to_dir) == -1) {
            fprintf(stderr, "mysh: An error occurred while executing internal command \"cd\"\n");
            perror("cd");
        }
        return true;
    }
    return false;
}

/*
 * Execute a linked list of external commands, and handle piping between the
 * commands.  Skip any internal commands that might exist in the linked list.
 */
void handle_external_commands(command * cmd, int * left_pipe) {
    if (!cmd) {
        return;
    }

    int input_fd;
    int output_fd;
    int right_pipe[2];
    pid_t cpid;

    if (cmd->next) {
        if (pipe(right_pipe) == -1) {
            fprintf(stderr, "mysh: A fatal error occurred\n");
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    cpid = fork();   
    if (cpid < 0) {
        fprintf(stderr, "mysh: A fatal error occurred\n");
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (cpid == 0) {
        // Child process
        // Determine where to get input
        if (cmd->input) {
            assert(!left_pipe);
            input_fd = open(cmd->input, O_RDONLY | O_CLOEXEC);
            if (input_fd < 0) {
                // TODO(agf): Note that error messages are taken from fish
                fprintf(stderr, "mysh: An error occurred while redirecting file \"%s\"\n", cmd->input);
                perror("open");
                // TODO(agf): The child process exiting, does not cause the
                // parent to exit
                exit(EXIT_FAILURE);
            }
            dup2(input_fd, STDIN_FILENO);
        } else if (left_pipe) {
            // Use the read end instead of STDIN
            dup2(left_pipe[0], STDIN_FILENO);
            // Close the unused write end
            close(left_pipe[1]);
        }

        // Determine where to send output
        if (cmd->output) {
            assert(!cmd->next);
            output_fd = open(cmd->output, O_CREAT | O_WRONLY | O_CLOEXEC);
            if (output_fd < 0) {
                fprintf(stderr, "mysh: An error occurred while redirecting to file \"%s\"\n", cmd->output);
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(output_fd, STDOUT_FILENO);
        } else if (cmd->next) {
            // Close unused read end
            close(right_pipe[0]);
            // Use the write end instead of STDOUT
            dup2(right_pipe[1], STDOUT_FILENO);
        }
        execute_command(cmd);
        // If we reach this point, then the command failed to execute properly
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        if (left_pipe) {
            close(left_pipe[0]);
            close(left_pipe[1]);
        }
        // Parent process
        if (cmd->next) {
            handle_external_commands(cmd->next, right_pipe);
        }
        waitpid(cpid, NULL, 0); 
    }
}

/*
 * Determine if |cmd| is an internal or external command, and call the proper
 * function to execute it.
 */
void handle_commands(command * cmd) {
    if (!handle_internal_command(cmd)) {
        handle_external_commands(cmd, NULL);
    }
}

/* Print a prompt containing the username and current working directory. */
void print_prompt() {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("%s:%s>", getlogin(), cwd);
}

char * copy_except_x(char * dst, char * src, int len, char bad) {
    int i, j = 0;
    for (i = 0; i < len; i++) {
        if (src[i] != bad) {
            dst[j] = src[i];
            j++;
        }
    }
    dst[j] = '\0';
    return dst;
}

/*
 * Given an array of string pointers, a source string, and the number of tokens
 * to make from the string,
 * create new strings to represent each token and set the array elements
 * to point to them. 
 * Recognize newlines and string termination characters as end of string.
 * If too many or too few tokens are found, report the error to stderr and exit.
 */
void split_cmd(char ** dst, char * src, char * end, int n_tokens) {
    char *iter = src;
    char *begin = src;
    int token_no = 0;
    char seen_char = 0;

    while (iter <= end) {

        /* if double quote, skip to next double quote */
        if (*iter == '"') {
            iter++;
            while (*iter != '"') {
                iter++;
            }
            seen_char = 1;
            iter++;
        }

        /* whitespace */
        else if (*iter == ' ' || *iter == '\t' || 
                 *iter == '\n' || *iter == '\0' || iter == end) {
            /* if leading whitespace, crop it */
            if (seen_char == 0) {
                iter++;
                begin++;
            }
            /* otherwise, copy the preceding token into the array */
            else {
                if (token_no >= n_tokens) {
                    fprintf(stderr, "Too many tokens for array\n");
                    exit(1);
                }

                int len = (iter - begin);
                dst[token_no] = (char *) malloc(len + 1);
                copy_except_x(dst[token_no], begin, len, '"');

                token_no++;
                iter++;
                begin = iter;
                seen_char = 0;
            }
        }

        /* if other character, set seen_char and increment iter */
        else {
            seen_char = 1;
            iter++;
        }
    }

    if (token_no < n_tokens) {
        fprintf(stderr, "Not enough tokens for array\n");
        exit(1);
    }
}

/*
 * Given a string from the command line, translate it into a linked list
 * of cmd_struct's.
 * First tokenize the string according to whitespace and double quotes,
 * then structurize the tokens,
 * structures demarked by pipes ("|") and redirects ("<", ">").
 */
command * structure_cmds(char * cmd_str) {
    char *begin = cmd_str;
    char *iter = cmd_str;
    /* possible states are adding a command (0), 
     * redirecting input (1), and redirecting output (2)
     */
    char state = 0;
    char seen_char = 0;
    int token_counter = 0;
    command *head = NULL;
    command *tail = NULL;

    while (iter - cmd_str < strlen(cmd_str)) {

        /* if double quote, skip to next double quote */
        if (*iter == '"') {
            iter++;
            /* if no closing quote appears, exit */
            while (*iter != '"') {
                if (*iter == '\0') {
                    fprintf(stderr, "Mismatched quotes\n");
                    return NULL;
                }
                iter++;
            }
            seen_char = 1;
        }

        /* if whitespace */
        else if (*iter == ' ' || *iter == '\t') {
            /* if non-whitespace has passed, count a token */
            if (seen_char) {
                token_counter++;
                seen_char = 0;
            }
            /* otherwise nothing (in particular, do not set seen_char to 1) */
        }

        /* check for pipes and redirects and end of string */
        else if (*iter == '|' || *iter == '<' || *iter == '>' || 
                 *iter == '\0' || *iter == '\n') {

            /* end of one state */
            if (seen_char) {
                token_counter++;
            }

            /* new command */
            if (state == 0) {

                if (token_counter == 0) {
                    fprintf(stderr, "Can't pipe to empty command\n");
                    return 0;
                }
                /* create new command and set to tail */
                if (head == NULL) {
                    head = tail = new_command();
                }
                else {
                    tail->next = new_command();
                    tail = tail->next;
                }

                /* set tokens array */
                tail->tokens = 
                    (char **) malloc(sizeof(char *) * (token_counter + 1));
                split_cmd(tail->tokens, begin, iter, token_counter);
                /* set final element of tokens array to NULL */
                tail->tokens[token_counter] = NULL;
            }

            /* redirect input */
            else if (state == 1) {
                if (token_counter != 1) {
                    fprintf(stderr, "Input must be one token\n");
                    return 0;
                }
                if (tail != head) {
                    fprintf(stderr, "Only 1st command takes input redirect\n");
                    return 0;
                }

                /* strip leading whitespace */
                while (isspace(*begin))
                    begin++;
                /* strip trailing whitespace */
                char * marker = iter - 1;
                while (isspace(*marker))
                    marker--;

                /* copy string */
                int len = marker - begin + 1;
                tail->input = (char *) malloc(len + 1);
                copy_except_x(tail->input, begin, len, '"');
                tail->input[len] = '\0';
            }

            /* redirect output */
            else {
                if (token_counter != 1) {
                    fprintf(stderr, "Output must be one token\n");
                    return 0;
                }

                /* strip leading whitespace */
                while (isspace(*begin))
                    begin++;
                /* strip trailing whitespace */
                char * marker = iter - 1;
                while (isspace(*marker))
                    marker--;

                /* copy string */
                int len = marker - begin + 1;
                tail->output = (char *) malloc(len + 1);
                copy_except_x(tail->output, begin, len, '"');
                tail->output[len] = '\0';

            }

            /* set new state */
            if (*iter == '|') {
                state = 0;
                if (tail->output != NULL) {
                    fprintf(stderr, "Only end command takes output redirect\n");
                    return 0;
                }
            }
            else if (*iter == '<') {
                state = 1;
            }
            else if (*iter == '>') {
                state = 2;
            }
            else {
                return head;
            }

            begin = iter + 1;
            seen_char = 0;
            token_counter = 0;
        }

        /* for any other character, set seen_char to 1 */
        else {
            seen_char = 1;
        }

        iter++;
    }
    return NULL;
}

void test1() {
    command * cmd1 = new_command();
    cmd1->tokens = malloc(4 * sizeof(char *));
    cmd1->tokens[0] = "grep";
    cmd1->tokens[1] = "fork";
    cmd1->tokens[2] = "/home/aaron/Dropbox/Caltech/WIN_2016/CS124/nos/src/shell/mysh.c";
    cmd1->tokens[3] = NULL;
    handle_commands(cmd1);
}

void test2() {   
    command * cmd1 = new_command();
    command * cmd2 = new_command();
    cmd1->next = cmd2;
    cmd1->tokens = malloc(3 * sizeof(char *));
    cmd1->tokens[0] = "cat";
    cmd1->tokens[1] = "/home/aaron/Dropbox/Caltech/WIN_2016/CS124/nos/src/shell/mysh.c";
    cmd1->tokens[2] = NULL;
    cmd2->tokens = malloc(3 * sizeof(char *));
    cmd2->tokens[0] = "grep";
    cmd2->tokens[1] = "fork";
    cmd2->tokens[2] = NULL;
    handle_commands(cmd1);
}

void test3() {
    command * cmd1 = new_command();
    command * cmd2 = new_command();
    command * cmd3 = new_command();
    cmd1->next = cmd2;
    cmd2->next = cmd3;
    cmd1->tokens = malloc(3 * sizeof(char *));
    cmd1->tokens[0] = "cat";
    cmd1->tokens[1] = "/home/aaron/Dropbox/Caltech/WIN_2016/CS124/nos/src/shell/mysh.c";
    cmd1->tokens[2] = NULL;
    cmd2->tokens = malloc(3 * sizeof(char *));
    cmd2->tokens[0] = "grep";
    cmd2->tokens[1] = "max";
    cmd2->tokens[2] = NULL;
    cmd3->tokens = malloc(3 * sizeof(char *));
    cmd3->tokens[0] = "grep";
    cmd3->tokens[1] = "len";
    cmd3->tokens[2] = NULL;
    handle_commands(cmd1);
}

void test4() {
    command * cmd1 = new_command();
    command * cmd2 = new_command();
    command * cmd3 = new_command();
    command * cmd4 = new_command();
    cmd1->next = cmd2;
    cmd2->next = cmd3;
    cmd3->next = cmd4;
    cmd1->tokens = malloc(3 * sizeof(char *));
    cmd1->tokens[0] = "cat";
    cmd1->tokens[1] = "/home/aaron/Dropbox/Caltech/WIN_2016/CS124/nos/src/shell/mysh.c";
    cmd1->tokens[2] = NULL;
    cmd2->tokens = malloc(3 * sizeof(char *));
    cmd2->tokens[0] = "grep";
    cmd2->tokens[1] = "max";
    cmd2->tokens[2] = NULL;
    cmd3->tokens = malloc(3 * sizeof(char *));
    cmd3->tokens[0] = "grep";
    cmd3->tokens[1] = "len";
    cmd3->tokens[2] = NULL;
    cmd4->tokens = malloc(3 * sizeof(char *));
    cmd4->tokens[0] = "grep";
    cmd4->tokens[1] = "str";
    cmd4->tokens[2] = NULL;
    handle_commands(cmd1);
}

void mainloop() {
    /* maximum bytes in an input line */
    int max_len = 1024;
    char cmd_str[max_len];

    command * cmd_list;
    while(1) {
        print_prompt();

        fgets(cmd_str, max_len, stdin);

        cmd_list = structure_cmds(cmd_str);
        handle_commands(cmd_list);
        cleanup_commands(cmd_list);
    }
}

int main(void) {
    mainloop();
    // test1();
    // test2();
    // test3();
    // test4();
    return 0;
}
