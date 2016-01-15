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

void get_prompt();
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

    while(1)
    {
        get_commands(command_buffer);
        parsed_commands *cmds = parse_commands(command_buffer);
        exec_commands_list(cmds);  
        free_parsed_commands(cmds);
    }
}

void print_prompt()
{
    /*
    Generates a command prompt.

    The format used is username:CWD>>> where CWD is the current working directory
    We are currently using the getcwd() command to get that directory. This might have
    to change when we use cd, depending on how cd is implemented
    */
    char * name = getlogin();
    char * directory = getcwd(NULL, 0);
    printf("%s:%s>>> ", name, directory);
}

// Gets characters from stdin into the command buffer until a newline comes in
// As per assignment, assuming all commands are < 1 KiB
void get_commands(char *command_buffer)
{
    print_prompt();
    // Getting multiline commands from here doesn't sound too bad if we read the string
    // in this function as well.
    char temp[MAX_CMD_SIZE];
    command_buffer[0] = '\0';
    do
    {
        fgets(temp, MAX_CMD_SIZE, stdin);
        strcat(command_buffer, temp);

    } while (temp[strlen(temp)-2] == '\\');
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
    if(i>0)
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
                    malloc(sizeof(char) * strlen(curr_token));
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

//     for (i = 0; i < answer->num_commands; ++i)
//     {
//         j = 0;
//         while(answer->commands[i][j] != '\0')
//         {
//             printf("%s:", answer->commands[i][j]);
//             j ++;
//         }
//         printf("\n");
//     }

    // // DEBUG: Tests redirection

//     printf("Input: %s\n", answer->file_in);
//     printf("Output: %s\n", answer->file_out);
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
            REPORT_ERR("wait failed");
        }
    }
}

// Here's the hard work. Spawn all the child processes, piping and redirecting 
// appropriately. Then wait for them to finish and return
void exec_commands_list(parsed_commands *cmds)
{
    // Special cases:
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
    int in_fd = (cmds->file_in != NULL) ? open(cmds->file_in, O_RDONLY) : -2;
    if (in_fd == -1)
    {
        REPORT_ERR("Couldn't open infile");
        close_pipes(pipes, num_pipes);
        return;
    }
    int out_fd = (cmds->file_out != NULL) ? open(cmds->file_in, O_WRONLY | O_CREAT) : -2;
    if (out_fd == -1)
    {
        REPORT_ERR("Couldn't open outfile");
        close_pipes(pipes, num_pipes);
        if (close(in_fd) == -1)
        {
            REPORT_ERR("Couldn't close infile.. that's ok I'm abandoning this command anyways");
        }
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
            if (in_fd != -2 && close(in_fd) == -1)
            {
                REPORT_ERR("Couldn't close infile on cleanup");
            }
            if (out_fd != -2 && close(out_fd) == -1)
            {
                REPORT_ERR("Couldn't close outfile on cleanup");
            }
            wait_for_children(child_pids, i);
            return;
        }
        if (child_pid == 0) // am child
        {
            // Set up redirects
            if (i == 0) // am first child
            {
                // redirect from file_in
                if (in_fd != -2)
                {
                    dup2(in_fd, STDIN_FILENO);
                }
            }
            if (i == cmds->num_commands - 1) // am last child
            {
                // redirect to file out
                if (out_fd != -2)
                {
                    dup2(out_fd, STDOUT_FILENO);
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

    wait_for_children(child_pids, cmds->num_commands);
    close_pipes(pipes, cmds->num_commands - 1);
    if (in_fd != -2 && close(in_fd) == -1)
    {
        REPORT_ERR("Couldn't close infile on cleanup");
    }
    if (out_fd != -2 && close(out_fd) == -1)
    {
        REPORT_ERR("Couldn't close outfile on cleanup");
    }
}


