#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

//Constants
#define MAX_CMD_SIZE 1024
// Since doing this in a function can mess up errno maybe
#define REPORT_ERR(error) {int errsv = errno; \
                           fprintf(stderr, error); \
                           fprintf(stderr, ", errno=%d", errsv);};

typedef struct
{
    char *binary;
    char **args;
} command;

typedef struct
{
    char *file_in;
    char *file_out;
    command *commands;
    int num_commands;
} parsed_commands;

char *get_prompt();
void get_commands(char *command_buffer);
parsed_commands *parse_commands(char *command_buffer);
int build_pipes(int num_commands, int (*pipes)[2]);
void free_parsed_commands(parsed_commands *cmds);
void exec_command(command cmnd);
void exec_commands_list(parsed_commands *cmds, int (*pipes)[2]);


int main(int argc, char const *argv[])
{
    char command_buffer[MAX_CMD_SIZE];

    while(1)
    {
        get_commands(command_buffer);
        parsed_commands *cmds = parse_commands(command_buffer);
        int pipes[cmds->num_commands][2];
        if (build_pipes(cmds->num_commands, pipes) != -1)
            exec_commands_list(cmds, pipes);  
        free_parsed_commands(cmds);
    }
}

char *get_prompt()
{
    /*
    Generates a command prompt.

    The format used is username:CWD>>> where CWD is the current working directory
    We are currently using the getcwd() command to get that directory. This might have
    to change when we use cd, depending on how cd is implemented
    */
    char * name = getlogin();
    char * directory = getcwd(NULL, 0);
    char * prompt = strcat(name, ":");
    prompt = strcat(prompt, directory);
    prompt = strcat(prompt, ">>>");
    return prompt;
}

// Pound characters from stdin into the command buffer until a newline comes in
// As per assignment, assuming all commands are < 1 KiB
void get_commands(char *command_buffer)
{
    printf("%s\n", get_prompt());
    fgets(command_buffer, MAX_CMD_SIZE, stdin);
}

// Build a parsed commands struct from the command in the command buffer
parsed_commands *parse_commands(char *command_buffer)
{
    //TODO
    return NULL;
}

// Make all the pipes
int build_pipes(int num_commands, int (*pipes)[2])
{
    int i;
    for (i = 0; i < num_commands - 1; i++)
    {
        if (pipe(pipes[i]) == -1)
        {
            REPORT_ERR("Couldn't open pipe");
            return -1; // Failure
        }
    }
    return 0; // Success
}

// Does what it says
void free_parsed_commands(parsed_commands *cmds)
{
    //TODO
}

// Here's the hard work. Spawn all the child processes, piping and redirecting 
// appropriately. Then wait for them to finish and return
void exec_commands_list(parsed_commands *cmds, int (*pipes)[2])
{
    // Special cases:
    // In these special cases 'binary' is a misnomer, but otherwise in general
    // it makes it more clear what's going on
    if (strcmp(cmds->commands[0].binary, "cd") == 0 
            || strcmp(cmds->commands[0].binary, "chdir") == 0)
    {
        // This assumes it's just given one argument. May want to add more 
        // error checking
        if (chdir(cmds->commands[0].args[0]) == -1)
        {
            REPORT_ERR("Couldn't change directory");
        }
        return;
    }
    if (strcmp(cmds->commands[0].binary, "exit") == -1)
    {
        // This one's easy!
        free_parsed_commands(cmds);
        exit(0);
    }
    if (strcmp(cmds->commands[0].binary, "make_me_a_sandwich") == 0)
    {
        // ascii from http://www.ascii-art.de/ascii/s/sandwich.txt
        printf("                    _.---._\
                _.-~       ~-._\
            _.-~               ~-._\
        _.-~                       ~---._\
    _.-~                                 ~\\\
 .-~                                    _.;\
 :-._                               _.-~ ./\
 }-._~-._                   _..__.-~ _.-~)\
 `-._~-._~-._              / .__..--~_.-~\
     ~-._~-._\\.        _.-~_/ _..--~~\
         ~-. \\`--...--~_.-~/~~\
            \\.`--...--~_.-~\
              ~-..----~");
        return;
    }

    // else it's a list of binaries to pipe together, along with the redirects
    // TODO the rest of this
    /* 
       fork each child from the parent, pipe them, redirect them
     */
    int child_pids[cmds->num_commands];
    int i;
    for (i = 0; i < cmds->num_commands; i++)
    {
        int child_pid = fork();
        if (child_pid == -1)
        {
            REPORT_ERR("Fork failed");
            //TODO ohshit what do we do... do children wait to run until they
            // know somehow that all forks succeeded?
        }
        if (child_pid == 0) // am child
        {
            // Set up redirects
            if (i == 0) // am first child
            {
//                dup2(, STDIN_FILENO);
            }
            if (i == cmds->num_commands - 1) // am last child
            {
//                dup2(, STDOUT_FILENO);
            }
            
            // Set up pipes
//            dup2(, STDIN_FILENO);
//            dup2(, STDOUT_FILENO);

            // And run
//            execlp();
              // children should never ever get here
        }
        // else am still command line
        child_pids[i] = child_pid;
    }
    // Only the command line gets here
    // Wait for all the children to finish, then return
}
