#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>

//Constants
#define MAX_CMD_SIZE 1024
// Since doing this in a function can mess up errno maybe
#define REPORT_ERR(error) {int errsv = errno; \
                           fprintf(stderr, error); \
                           fprintf(stderr, ", errno=%d\n", errsv);};
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

void get_commands(char *command_buffer);
parsed_commands *parse_commands(char *command_buffer);
int build_pipes(int (*pipes)[2], int num_pipes);
void close_pipes(int (*pipes)[2], int num_pipes);
void free_parsed_commands(parsed_commands *cmds);
void exec_command(char ** cmnd);
void exec_commands_list(parsed_commands *cmds);


int main(int argc, char const *argv[])
{
    char command_buffer[MAX_CMD_SIZE];
    using_history();

    while(1)
    {
        get_commands(command_buffer);
        parsed_commands *cmds = parse_commands(command_buffer);
        exec_commands_list(cmds);  
        free_parsed_commands(cmds);
    }
}

// Gets characters from stdin into the command buffer until a newline comes in
// As per assignment, assuming all commands are < 1 KiB
void get_commands(char *command_buffer)
{
    char * name = getlogin();
    char * directory = getcwd(NULL, 0);
    char prompt[sizeof(char) * (strlen(name) + strlen(directory) + 6)]; // extra terms for >>> etc.
    prompt[0] = '\0';
    strcat(prompt, name);
    strcat(prompt, ":");
    strcat(prompt, directory);
    strcat(prompt, ">>> ");
    free(directory);


    char * line = NULL;
    command_buffer[0] = '\0';
    do
    {
        if(line != NULL)
        {
                free(line);
        }
        line = readline(prompt);
        // There seem to be "still reachable" memleaks coming from here
        // We believe this might come from the readline library. 
        add_history(line);
        strcat(command_buffer, line);
        // printf("print %c\n", line[strlen(line) -1]);
        prompt[0] = '\0';

    } while (line[strlen(line)-1] == '\\');
    strcat(command_buffer, "\n");
    free(line);
}
/**
This function finds the next token of buffer starting at position i
and places it in dest. It returns the position at which the token ends

There are some awkward issues with dealing with quotes.
**/
int get_next_token(char *dest, char* buffer, int i)
{
    // We want to strip out the quote so we need to skip one
    if (buffer[i] == '"')
    {
        i ++;
    }
    int j = i;
    // Check whether we are in a token that starts with a quote
    bool quoted = false;
    if(i > 0)
    {
        quoted = (buffer[i-1] == '"');
    }
    // Loop through the buffer until we hit a special character
    for (;i < strlen(buffer); ++i)
    {
        // The empty string should never be a bin or an arg
        // The args should be separated by spaces regardless of quotes
        // e.g. "a""b" is not valid but "a" "b" is.
       if (buffer[i] == '"' || (!quoted && (buffer[i] == ' ' || buffer[i] == '\t')) || 
            buffer[i] == '>' || buffer[i] == '<' || buffer[i] == '|' || buffer[i] == '\0')
        {
            break;
        }
    }
    strncpy(dest, buffer + j, i-j);
    // Make sure to null terminate
    dest[i-j] = '\0';
    if (buffer[i] == '"')
    {
        i ++;
    }
    return i;
}
/**
This function removes the whitespace from an input string and puts the 
result into new_command_buffer.
Also stips out the \ character and newlines.
**/
void strip_whitespace(char *command_buffer, char *new_command_buffer)
{
    int i,j;
    bool quotes_mode = false;
    bool copy_these[MAX_CMD_SIZE];
    // We want to remove whitespace around | < and >
    // By default we expect all characters to be in the new buffer
    for (i = 0; i < strlen(command_buffer); ++i)
    {
        copy_these[i] = true;
        if (command_buffer[i] == '\\' || command_buffer[i] == '\n')
        {
            copy_these[i] = false;
        }
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
        else if(command_buffer[i] == '|' || command_buffer[i] == '<' 
            || command_buffer[i] == '>')
        {
            // When we see a special character, remove spaces to the 
            // left and right until there are no more.
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
    // Manually add a null terminator so we can use strlen
    new_command_buffer[j] = '\0';

    // DEBUG: Prints the input without whitespace
    // printf("new command buffer: %s\n", new_command_buffer);
}

// Build a parsed commands struct from the command in the command buffer
parsed_commands *parse_commands(char *command_buffer)
{
    char new_command_buffer[MAX_CMD_SIZE];
    strip_whitespace(command_buffer, new_command_buffer);
    int i,j;

    parsed_commands *answer= malloc(sizeof(parsed_commands));
    if (answer == NULL)
    {
        REPORT_ERR("Malloc answer failed.");
        exit(1);
    }
    answer->num_commands = 0;
    answer->file_in = NULL;
    answer->file_out = NULL;
    answer->commands = malloc(sizeof(char *) * MAX_CMD_SIZE/2); //list of commands
    if (answer->commands == NULL)
    {
        REPORT_ERR("Malloc answer->commands failed");
        exit(1);
    }

    char curr_token[1024]; // tokens are space separated bin,args, etc.
    i = 0;
    while (i < strlen(new_command_buffer))
    {   
        // When we start a new command
        if (new_command_buffer[i] == '|' || i == 0)
        {
            answer->commands[answer->num_commands] = malloc(sizeof(char *) * MAX_CMD_SIZE/2);
            if (answer->commands[answer->num_commands] == NULL)
            {
                REPORT_ERR("Malloc answer->commands[answer->num_commands] failed");
                exit(1);
            }
            j = 0;
            do
            {
                // We get the next token. We add an extra one to get rid of the white space
                // args (and bins) are separated by whitespace, but we don't want that in 
                // out tokens. So we make sure to use i+1 when it isn't the first command.
                i = get_next_token(curr_token, new_command_buffer, i + (i != 0));
                // DEBUG: This prints the current token.
                // printf("curr_token: %s\n", curr_token);
                answer->commands[answer->num_commands][j] = 
                    malloc(sizeof(curr_token));
                if (answer->commands[answer->num_commands][j] == NULL)
                {
                    REPORT_ERR("Malloc answer->commands[answer->num_commands][j] failed");
                    exit(1);
                }
                answer->commands[answer->num_commands][j] = 
                    strcpy(answer->commands[answer->num_commands][j], curr_token);
                j ++;
            } while(new_command_buffer[i] != '|' && new_command_buffer[i] != '<' && 
                new_command_buffer[i] != '>' && new_command_buffer[i] != '\0' &&
                !(new_command_buffer[i] == '"' && new_command_buffer[i+1] == '\0'));

            // If we end on a quote, then we need to make sure to not
            // include it as part of the arg value, or try to make it its own token.
            if (new_command_buffer[i] == '"' && new_command_buffer[i+1] == '\0')
            {
                i ++;
            }
            // Make sure to= NULL terminate for strlen
            answer->commands[answer->num_commands][j] = '\0';
            answer->num_commands += 1;
        }

        // In case of redirects, get the token, and put it into our parsed command 
        // struct.
        // There should only be one > and one < in a line.
        if (new_command_buffer[i] == '<')
        {
            i = get_next_token(curr_token, new_command_buffer, i+1);
            answer->file_in = malloc(sizeof(char) * strlen(curr_token));
            if (answer->file_in == NULL)
            {
                REPORT_ERR("Malloc answer->file_in failed");
                exit(1);
            }
            answer->file_in = strcpy(answer->file_in, curr_token);
        }
        if (new_command_buffer[i] == '>')
        {
            i = get_next_token(curr_token, new_command_buffer, i+1);
            answer->file_out = malloc(sizeof(char) * strlen(curr_token));
            if (answer->file_out == NULL)
            {
                REPORT_ERR("Malloc answer->file_out failed");
                exit(1);
            }
            answer->file_out = strcpy(answer->file_out, curr_token);
        }
    }

    // DEBUG:
    // Prints out everything that we are doing on that line
    // Each Command is on a new line and commands and args are
    // separated by ":"

     // for (i = 0; i < answer->num_commands; ++i)
     // {
     //     j = 0;
     //     while(answer->commands[i][j] != '\0')
     //     {
     //         printf("%s:", answer->commands[i][j]);
     //         j ++;
     //     }
     //     printf("\n");
     // }

    // // DEBUG: Tests redirection

    //  printf("Input: %s\n", answer->file_in);
    //  printf("Output: %s\n", answer->file_out);

    return answer;
}

// Make all the pipes
int build_pipes(int (*pipes)[2],int num_pipes)
{
    int i;
    for (i = 0; i < num_pipes; i++)
    {
        if (pipe(pipes[i]) == -1)
        {
            REPORT_ERR("Couldn't open pipe");
            // Clean up pipes already made
            close_pipes(pipes, i);
            return -1; // Failure
        }
    }
    return 0; // Success
}

// Does what it says
void free_parsed_commands(parsed_commands *cmds)
{
    int i, j = 0;
    // iterate through commands array
    for (i = 0; i < cmds->num_commands; i++)
    {
        // iterate through and free strings
        while(cmds->commands[i][j] != NULL)
        {
            // swim free, my pretties
            free(cmds->commands[i][j]);
            j++;
        }
        free(cmds->commands[i]); // free array of strings
    }
    free(cmds->commands); // free giant commands array
    free(cmds->file_in);
    free(cmds->file_out);
    free(cmds);
}

void close_pipes(int (*pipes)[2], int num_pipes)
{
    int i = 0;
    for (i = 0; i < num_pipes; i++)
    {
        if (close(pipes[i][0]) == -1)
        {
            REPORT_ERR("pipe closing failed");
        }
        if (close(pipes[i][1]) == -1)
        {
           REPORT_ERR("pipe closing filed");
        }
    }
}

void wait_for_children(int *child_pids, int num_children)
{
    int i;
    for (i = 0; i < num_children; i++)
    {
        int status;
        int w = waitpid(child_pids[i], &status, 0);
        if (w != child_pids[i])
        {
            if (errno != ECHILD) // if it's not just because the child is already dead
                REPORT_ERR("wait failed");
        }
    }
}

// Here's the hard work. Spawn all the child processes, piping and redirecting 
// appropriately. Then wait for them to finish and return
void exec_commands_list(parsed_commands *cmds)
{
    // Special cases:
    if (cmds->num_commands == 0)
    {
        return;
    }
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
    if (strcmp(cmds->commands[0][0], "exit") == 0)
    {
        // This one's easy!
        free_parsed_commands(cmds);
        HIST_ENTRY ** temp = history_list();
        for(; *temp != NULL; temp++)
        {
            free_history_entry(*temp);
        }


        exit(0);
    }
    if (strcmp(cmds->commands[0][0], "make_me_a_sandwich") == 0)
    {
        // ascii from http://www.ascii-art.de/ascii/s/sandwich.txt
        printf("\
                    _.---._\n\
                _.-~       ~-._\n\
            _.-~               ~-._\n\
        _.-~                       ~---._\n\
    _.-~                                 ~\\\n\
 .-~                                    _.;\n\
 :-._                               _.-~ ./\n\
 }-._~-._                   _..__.-~ _.-~)\n\
 `-._~-._~-._              / .__..--~_.-~\n\
     ~-._~-._\\.        _.-~_/ _..--~~\n\
         ~-. \\`--...--~_.-~/~~\n\
            \\.`--...--~_.-~\n\
              ~-..----~\n");
        return;
    }

    // else it's a list of binaries to pipe together, along with the redirects

    // First open all the pipes and redirect files
    int num_pipes = cmds->num_commands - 1;
    int pipes[num_pipes][2];
    if (build_pipes(pipes, num_pipes) == -1)
    {
        REPORT_ERR("Couldn't build pipes");
        return;
    } 
    
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
           REPORT_ERR("Fork failed, abandoning command");
            // murder all pipes, wait for all children to die, then return
            close_pipes(pipes, num_pipes);
            wait_for_children(child_pids, i);
            return;
        }
        if (child_pid == 0) // am child
        {
            // Set up redirects
            // am first child and need redirection
            if (i == 0 && cmds->file_in != NULL) 
            {
                int in_fd = open(cmds->file_in, O_RDONLY);
                if (in_fd == -1)
                {
                    REPORT_ERR("Couldn't open infile");
                    close_pipes(pipes, num_pipes);
                    return;
                }
                dup2(in_fd, STDIN_FILENO);
                if (close(in_fd) == -1)
                {
                    REPORT_ERR("Couldn't close infile after setting up redirect");
                }
 
            }
            // am last child and need redirection
            if (i == cmds->num_commands - 1 && cmds->file_out != NULL) 
            {
                int out_fd = open(cmds->file_out, O_WRONLY | O_CREAT);
                if (out_fd == -1)
                {
                    REPORT_ERR("Couldn't open outfile");
                    close_pipes(pipes, num_pipes);
                    return;
                }
                dup2(out_fd, STDOUT_FILENO);
                if (close(out_fd) == -1)
                {
                    REPORT_ERR("Couldn't close outfile after setting up redirect");
                }
            }
            // piping!
            if (i != 0)
            {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            if (i != cmds->num_commands - 1)
            {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            
            // Clean up the extra file descriptors and open files
            close_pipes(pipes, cmds->num_commands - 1);

            // And run
            execvp(cmds->commands[i][0], cmds->commands[i]);
              // children should never ever get here
            // check for failure
            // halp halp what do
        }
        // else am still command line
        child_pids[i] = child_pid;
    }
    // Only the command line gets here
    // Wait for all the children to finish, then return

    close_pipes(pipes, cmds->num_commands - 1);
    wait_for_children(child_pids, cmds->num_commands);
    
}


