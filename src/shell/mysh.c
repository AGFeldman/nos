#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

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

/* Print a prompt containing the username and current working directory. */
void print_prompt();

/*
 * Given a string from the command line, translate it into a linked list
 * of cmd_struct's.
 * First tokenize the string according to whitespace and double quotes,
 * then structurize the tokens,
 * structures demarked by pipes ("|") and redirects ("<", ">").
 */
command * structure_cmds(char *);

/*
 * Given an array of string pointers, a source string, and the number of tokens
 * to make from the string,
 * create new strings to represent each token and set the array elements
 * to point to them. 
 * Recognize newlines and string termination characters as end of string.
 * If too many or too few tokens are found, report the error to stderr and exit.
 */
void split_cmd(char ** dst, char * src, char * end, int n_tokens);

int main(void) {
    /* maximum bytes in an input line */
    int max_len = 1024;
    char cmd_str[max_len];

    command * cmd_list;
    while(1) {
        print_prompt();

        fgets(cmd_str, max_len, stdin);

        cmd_list = structure_cmds(cmd_str);
        print_cmd_list(cmd_list);
    }


    return 0;
}

void print_prompt() {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("%s:%s>", getlogin(), cwd);
}

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
                int len = iter - begin;
                tail->input = (char *) malloc(len + 1);
                memcpy(tail->input, begin, len);
                tail->input[len] = '\0';
            }

            /* redirect output */
            else {
                if (token_counter != 1) {
                    fprintf(stderr, "Output must be one token\n");
                    return 0;
                }
                int len = iter - begin;
                tail->output = (char *) malloc(len + 1);
                memcpy(tail->output, begin, len);
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
}

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
                memcpy(dst[token_no], begin, len);
                dst[token_no][len] = '\0';

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















