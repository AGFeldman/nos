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
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_LOGIN_CHARS 80
#define MAX_CWD_CHARS 1024
#define MAX_EXTRA_PROMPT_CHARS 10

/*
 * Represents a shell command and contains a pointer to the next command, if 
 * any.
 * For example, represents `grep hi` or `grep hi < infile > outfile`, but does
 * not represent `grep hi | grep i`, because `grep hi | grep i` contains two
 * commands. But, the `next` member might point to a command struct that
 * represents `grep i`.
 */
typedef struct command {
    // A NULL-delimited array of tokens
    // None of these tokens are related to io redirection
    char ** tokens;
    // A name of a file, if input is redirected from a file. Otherwise, NULL.
    char * input;
    // A name of a file, if output is redirected to a file. Otherwise, NULL.
    char * output;
    // A pointer to the next command in a piped chain of commands, or NULL
    // if this is the last command in the sequence.
    struct command * next;
} command;

/*
 * Return a new command struct with all values set to NULL.
 * Memory for the returned struct needs to be freed later.
 */
command * new_command() {
    command * cmd = (command *) malloc(sizeof(command));
    cmd->tokens = NULL;
    cmd->input = NULL;
    cmd->output = NULL;
    cmd->next = NULL;
    return cmd;
}

/*
 * Print the contents of a linked list of command structs.
 * Useful for debugging.
 */
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
 * Free memory associated with a linked list of `command` structs.
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
        fprintf(stderr, 
                "mysh: An error occurred while executing command \"%s\"\n", 
                cmd->tokens[0]);
        perror("execve");
    }
}

/*
 * Handle internal `exit` command: Exit the shell.
 */
void command_exit() {
    exit(EXIT_SUCCESS);
}

/*
 * Handle internal `cd` command: Change the working directory
 */
void command_cd(command * cmd) {
    // tokens[1] is a valid dereference because tokens is always terminated by
    // NULL
    char * to_dir = cmd->tokens[1];
    if (to_dir == NULL) {
        to_dir = getenv("HOME");
    }
    if (chdir(to_dir) == -1) {
        fprintf(stderr, 
                "mysh: An error occurred while executing command \"cd\"\n");
        perror("cd");
    }
}

/*
 * Handle internal `history` command: Print history of commands entered into
 * the shell.
 */
void command_history() {
    HIST_ENTRY ** hist = history_list();
    if (hist) {
        while (*hist) {
            printf("%s\n", (**hist).line);
            hist++;
        }
    }
}

/*
 * If cmd->tokens represents an internal command, then execute it, handle any
 * errors, ignore any subsequent commands linked to by cmd->next, and return
 * true. Otherwise, return false.
 */
bool handle_internal_command(command * cmd) {
    if (!cmd) {
        return false;
    }
    char * first_token = cmd->tokens[0];
    if (strcmp(first_token, "exit") == 0) {
        command_exit();
        return true;
    } else if (strcmp(first_token, "cd") == 0) {
        command_cd(cmd);
        return true;
    } else if (strcmp(first_token, "history") == 0) {
        command_history();
        return true;
    }
    return false;
}

/*
 * Execute a linked list of external commands, and handle piping between the
 * commands.  Skip any internal commands that might exist in the linked list.
 * |left_pipe| is a pointer to a pair of file descriptors for a pipe that
 * appears to the left of |cmd| in the command sequence.
 */
void handle_external_commands(command * cmd, int * left_pipe) {
    if (!cmd) {
        return;
    }

    // File descriptor used for input redirection, if any
    int input_fd;
    // File descriptor used for output redirection, if any
    int output_fd;
    // Pair of file descriptors for a pipe to the next command, if any
    int right_pipe[2];
    pid_t cpid;

    // If this is not the last command in the sequence, then we need to create
    // a pipe to the next command.
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
            // Input is redirected from a file
            assert(!left_pipe);
            input_fd = open(cmd->input, O_RDONLY | O_CLOEXEC);
            if (input_fd < 0) {
                fprintf(stderr, 
                    "mysh: An error occurred while redirecting file \"%s\"\n", 
                    cmd->input);
                perror("open");
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
                fprintf(stderr, 
                    "mysh: An error occurred while redirecting to \"%s\"\n", 
                    cmd->output);
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

/*
 * Set |prompt| to contain a prompt string that includes the username and
 * current working directory.
 * |prompt| should point to at least
 *     MAX_LOGIN_CHARS+MAX_CWD_CHARS+MAX_EXTRA_PROMPT_CHARS
 * of allocated memory.
 */
void set_prompt(char * prompt) {
    char login[MAX_LOGIN_CHARS];
    if (getlogin_r(login, sizeof(login)) != 0) {
        fprintf(stderr, "mysh: Could not find login\n");
        perror("getlogin_r");
        login[0] = '\0';
    }
    char cwd[MAX_CWD_CHARS];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        fprintf(stderr, "mysh: Could not get current working directory\n");
        perror("getcwd");
        cwd[0] = '\0';
    }
    // It is important that the number of extra ":",">", etc. characters added
    // here, be less than MAX_EXTRA_PROMPT_CHARS
    sprintf(prompt, "%s:%s>", login, cwd);
}

/*
 * Copy up to n chars from src to dst, skipping any bad chars.
 * Terminate with an end-of-string character. 
 * Perform no checks of input, so will crash if dst is not long enough
 * to hold all non-bad chars of src and an end-of-string character.
 * Return dst.
 */
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
                assert(token_no < n_tokens);

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

    assert(token_no == n_tokens);
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

    while (iter - cmd_str <= strlen(cmd_str)) {

        /* if double quote, skip to next double quote */
        if (*iter == '"') {
            iter++;
            /* if no closing quote appears, exit */
            while (*iter != '"') {
                if (*iter == '\0') {
                    cleanup_commands(head);
                    fprintf(stderr, "mysh: Mismatched quotes\n");
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
                    cleanup_commands(head);
                    fprintf(stderr, "mysh: Can't pipe to empty command\n");
                    return NULL;
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
                    cleanup_commands(head);
                    fprintf(stderr, "mysh: Input must be one token\n");
                    return NULL;
                }
                if (tail != head) {
                    cleanup_commands(head);
                    fprintf(stderr, 
                            "mysh: Only 1st command takes input redirect\n");
                    return NULL;
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
                    cleanup_commands(head);
                    fprintf(stderr, "mysh: Output must be one token\n");
                    return NULL;
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
                    cleanup_commands(head);
                    fprintf(stderr, 
                            "mysh: Only end command takes output redirect\n");
                    return NULL;
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

void mainloop() {
    command * cmd_list;
    char * prompt = (char *)malloc((MAX_LOGIN_CHARS + 
                                    MAX_CWD_CHARS + 
                                    MAX_EXTRA_PROMPT_CHARS) * sizeof(char));
    char * cmd_str;
    while(1) {
        // Set prompt and read command into cmd_str
        set_prompt(prompt);
        cmd_str = readline(prompt);
        if (cmd_str == NULL) {
            continue;
        }
        if (cmd_str[0] != '\0') {
            add_history(cmd_str);
        }

        // Execute commands in cmd_str
        cmd_list = structure_cmds(cmd_str);
        free(cmd_str);
        handle_commands(cmd_list);
        cleanup_commands(cmd_list);
    }
    free(prompt);
}

int main(void) {
    mainloop();
    return 0;
}
