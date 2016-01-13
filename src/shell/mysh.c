#include <string.h>
#include <unistd.h>
#include <stdio.h>

//Constants
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define MAX_LEN 1024

char* get_prompt()
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




int main(int argc, char const *argv[])
{
    char input[MAX_LEN];
    while (1)
    {
        printf("%s\n", get_prompt());
        fgets(input, MAX_LEN, stdin);
        printf("You typed: %s\n", input);
    }
    return 0;
}