#include <stdlib.h>

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

int main(int argc, char ** argv) {
    return 0;
}
