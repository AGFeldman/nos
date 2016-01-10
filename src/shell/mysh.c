#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

typedef struct command {
    char * cmd;
    char * input;
    char * output;
    struct command * next;
} command;

command * new_command() {
    command * cmd = (command *) malloc(sizeof(command));
    cmd->cmd = NULL;
    cmd->input = NULL;
    cmd->output = NULL;
    cmd->next = NULL;
    return cmd;
}

/* Print a prompt containing the username and current working directory. */
void print_prompt();

/*
 * Given a string from the command line, translate it into a linked list
 * of cmd_struct's.
 * First tokenize the string according to whitespace and double quotes,
 * then structurize the tokens,
 * structures demarked by pipes ("|") and redirects ("<", ">").

cmd_struct *structure_cmds(char[]);
 */
void structure_cmds(char[]);

int main(void) {
    /* maximum bytes in an input line */
    int max_len = 1024;
    char cmd_str[max_len]; /* sketchy */
    while(1) {
        print_prompt();

        /* clear cmdstr? */
        fgets(cmd_str, max_len, stdin);

        structure_cmds(cmd_str);
    }


    return 0;
}

void print_prompt() {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("%s:%s>", getlogin(), cwd);
}

void structure_cmds(char cmd_str[]) {
    char *iter = cmd_str;
    int len = 0;
    while (iter < cmd_str + strlen(cmd_str) - 1) {
        len++;
        /* if double quote, skip to next double quote */
        if (!strcmp(*iter, "\"")) {
            len++;
            while (strcmp(*(iter++), "\"")) {
                len++;
                iter++;
            }
        }

        /* check for pipes */
        if (!strcmp(*iter, "|")) {
            
        }
        iter++;
    }
}
















