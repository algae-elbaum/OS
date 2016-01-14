#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

//Constants
#define MAX_CMD_SIZE 1024
// Since doing this in a function can mess up errno maybe
#define REPORT_ERR(error) {int errsv = errno; \
                           fprintf(stderr, error); \
                           fprintf(stderr, ", errno=%d", errsv);};
#define true 1
#define false 0


typedef _Bool bool;

typedef struct
{
    char *file_in;
    char *file_out;
    char ***commands;
    int num_commands;
} parsed_commands;

char *get_prompt();
void get_commands(char *command_buffer);
parsed_commands *parse_commands(char *command_buffer);
int build_pipes(int num_commands, int (*pipes)[2]);
void free_parsed_commands(parsed_commands *cmds);
void exec_command(char ** cmnd);
void exec_commands_list(parsed_commands *cmds, int (*pipes)[2]);


int main(int argc, char const *argv[])
{
    char command_buffer[MAX_CMD_SIZE];

    while(1)
    {
        get_commands(command_buffer);
        parsed_commands *cmds = parse_commands(command_buffer);
        // printf("%s\n", cmds->commands);
        // int pipes[cmds->num_commands][2];
        // if (build_pipes(cmds->num_commands, pipes) != -1)
        //     exec_commands_list(cmds, pipes);  
        // free_parsed_commands(cmds);
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
    // Getting multiline commands from here doesn't sound too bad if we read the string
    // in this function as well.
    fgets(command_buffer, MAX_CMD_SIZE, stdin);
}

int get_next_token(char *dest, char* buffer, int i)
{
    int j = i;
    bool quotes_mode = false;
    for (;i < strlen(buffer) - 1; ++i)
    {
        if(buffer[i] == '\"')
        {
            quotes_mode = !quotes_mode;
        }
        else if (!quotes_mode && (buffer[i] == ' ' || buffer[i] == '\t' || 
            buffer[i] == '>' || buffer[i] == '<' || buffer[i] == '|' || buffer[i] == '\0'))
        {
            break;
        }
    }
    strncpy(dest, buffer + j, i-j);
    dest[i] = '\0';
    return i;
}

// Build a parsed commands struct from the command in the command buffer
parsed_commands *parse_commands(char *command_buffer)
{
    //TODO
    int i;
    int j;
    bool quotes_mode = false;
    bool copy_these[MAX_CMD_SIZE];
    // remove whitespace around | < and >
    for (i = 0; i < strlen(command_buffer); ++i)
    {
        copy_these[i] = true;
    }
    // This will fail if the first character is a pipe or redirect
    // or if one of those is the last character
    // However those are syntax errors anyway, so we don't need to worry.
    for (i = 0; i < strlen(command_buffer); ++i)
    {
        if(command_buffer[i] == '\"')
        {
            quotes_mode = !quotes_mode;
        }
        else if(command_buffer[i] == '|' || command_buffer[i] == '<' || command_buffer[i] == '>')
        {
            j = 1;
            while (command_buffer[i-j] == ' ' || command_buffer[i-j] == '\t')
            {
                copy_these[i-j] = false;
                j ++;
            }
            j = 1;
            while (command_buffer[i+j] == ' ' || command_buffer[i+j] == '\t')
            {
                copy_these[i+j] = false;
                j ++;
            }
        }
    }
    char new_command_buffer[MAX_CMD_SIZE];
    // We subtract one to remove the newline character
    j = 0;
    for (i = 0; i < strlen(command_buffer) - 1; ++i)
    {
        if (copy_these[i])
        {
            new_command_buffer[j] = command_buffer[i];
            j ++;
        }
    }
    new_command_buffer[j] = '\0';

    //These three prints are for testing purposes
    printf("new command buffer: %s\n", new_command_buffer);

    // The following code is for actually doing the parsing. It's not yet complete

    parsed_commands *answer= malloc(sizeof(parsed_commands));
    answer->num_commands = 0;
    answer->commands = malloc(sizeof(char *) * MAX_CMD_SIZE/2); //list of commands

    char curr_token[1024]; // whatever space seperated string thing im on
    i = 0;
    while (i < strlen(new_command_buffer))
    {   
        if (new_command_buffer[i] == '|' || i == 0)
        {
            printf("gotta pipe\n");
            // take big command and move it to command list
            //then start new big command
            answer->commands[answer->num_commands] = malloc(sizeof(char *) * MAX_CMD_SIZE/2);
            //have to deep copy somehow???
            j = 0;
            do
            {
                i = get_next_token(curr_token, new_command_buffer, i + (new_command_buffer[i] == '|'));
                answer->commands[answer->num_commands][j] = malloc(sizeof(char) * strlen(curr_token));
                answer->commands[answer->num_commands][j] = strcpy(answer->commands[answer->num_commands][j], curr_token);
                // printf("%c\n", new_command_buffer[j]);
                j ++;
            } while(new_command_buffer[i] != '|' && new_command_buffer[i] != '<' && 
                new_command_buffer[i] != '>' && new_command_buffer[i] != '\0');

            answer->num_commands += 1;
            printf("nosmoking\n");
        }
        if (new_command_buffer[i] == '<')
        {
            i = get_next_token(curr_token, new_command_buffer, i+1);
            answer->file_in = malloc(sizeof(char) * strlen(curr_token));
            answer->file_in = strcpy(answer->file_in, curr_token);
        }
        if (new_command_buffer[i] == '>')
        {
            i = get_next_token(curr_token, new_command_buffer, i+1);
            answer->file_out = malloc(sizeof(char) * strlen(curr_token));
            answer->file_out = strcpy(answer->file_out, curr_token);
        }
    }
    for (i = 0; i < answer->num_commands; ++i)
    {
        printf("%s\n", answer->commands[i][0]);
    }
    // printf("%s\n", answer->commands[0][0]);
    return answer;
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
    if (strcmp(cmds->commands[0][0], "cd") == 0 
            || strcmp(cmds->commands[0][0], "chdir") == 0)
    {
        // This assumes it's just given one argument. May want to add more 
        // error checking
        if (chdir(cmds->commands[0][1]) == -1)
        {
            REPORT_ERR("Couldn't change directory");
        }
        return;
    }
    if (strcmp(cmds->commands[0][0], "exit") == -1)
    {
        // This one's easy!
        free_parsed_commands(cmds);
        exit(0);
    }
    if (strcmp(cmds->commands[0][0], "make_me_a_sandwich") == 0)
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
