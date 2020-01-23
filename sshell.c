#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <limits.h>

#define CMDLINE_MAX 512
#define ARGS_MAX 16
#define TOKEN_MAX 32
#define NUM_CMDS_MAX 4

// TO DO:
// add comments as needed
// add library func errors
// do report
// check that #includes are ok

struct Command {
    char main_command[TOKEN_MAX];
    char * args[ARGS_MAX + 1];
    int number_of_arguments;
    bool outputRedirected;
    bool errorRedirected;
    int fd;
};

struct Stack{
    char * directory;
    struct Stack * next;
};

/*
    This function initializes a new Command by allocating space for
    its' arguments array and setting the boolean to false.
*/
void initializeCommandStructure(struct Command * c1){
    //Malloc space for Command structure arguments and set arguments to null character
    for(int i = 0; i < ARGS_MAX + 1; i++){
        (*c1).args[i] = malloc(sizeof(char) * TOKEN_MAX);
        if((*c1).args[i] == NULL){
            fprintf(stderr, "malloc\n");
            exit(1);
        }
        memset((*c1).args[i], '\0', sizeof(char)* TOKEN_MAX);
    }//end for loop

    //Initialize Command structure
    (*c1).outputRedirected = false;
    (*c1).errorRedirected = false;
    return;
}//end function

/*
    This function adds the current char_buffer to the argument array in a Command.
    It then clears the char_buffer and updates the indices accordingly.
*/
void addBufferToArguments(char ** char_buffer, struct Command * c1, int * current_args_position, int * current_char_buffer_position, int * number_of_arguments){
    for(int j = 0; j < (int)strlen(*char_buffer); j++){
        (*c1).args[*current_args_position][j] = (*char_buffer)[j];
    }
    //Clear char_buffer and update indices
    memset(*char_buffer, '\0', sizeof(char)* TOKEN_MAX);
    *current_char_buffer_position = 0;
    (*current_args_position)++;
    (*number_of_arguments)++;
    return;
}//end function

/*
    This function parses the initial command into separate
    arguments defined by space,>, or &. It returns a Command
    structure after parsing. 
*/
struct Command parseInput(char * initial_input){
    //Malloc space for char_buffer and set it to null character
    char * char_buffer = malloc(sizeof(char) * TOKEN_MAX);
    if(char_buffer == NULL){
        fprintf(stderr, "malloc\n");
        exit(1);
    }
    memset(char_buffer, '\0', sizeof(char)* TOKEN_MAX);

    int current_char_buffer_position = 0;
    int current_args_position = 0;
    int number_of_arguments = 0;
    bool prev_char_space = false;
    bool output_file_argument_exists = false;

    struct Command c1;
    initializeCommandStructure(&c1);

    //Go through each character of the input given and parse it
    for(int i = 0; i < (int)strlen(initial_input); i++){
        if(initial_input[i] == ' '){
            if(prev_char_space == false){
                addBufferToArguments(&char_buffer, &c1, &current_args_position, &current_char_buffer_position, &number_of_arguments);
            }
            prev_char_space = true;
        }else if(initial_input[i] == '>'){
            //Output redirection
            if(i == 0){
                //Parsing error
                fprintf(stderr, "Error: missing command");
                c1.number_of_arguments = 0;
                return c1;
            }
            c1.outputRedirected = true;
            
            if(prev_char_space == false){
                addBufferToArguments(&char_buffer, &c1, &current_args_position, &current_char_buffer_position, &number_of_arguments);
            }
            prev_char_space = true;
            output_file_argument_exists = false;
        }else if(initial_input[i] == '&' && c1.outputRedirected == true && initial_input[i-1] == '>'){
            //Standard Error should be redirected for this Command
            c1.errorRedirected = true;
            prev_char_space = true;
            continue;
        }else{
            //Not a space character so add character to the char_buffer
            if(c1.outputRedirected == true){
                output_file_argument_exists = true;
            }

            char_buffer[current_char_buffer_position] = initial_input[i];
            current_char_buffer_position++;
            char_buffer = realloc(char_buffer, current_char_buffer_position);
            if(char_buffer == NULL){
                fprintf(stderr, "realloc\n");
                exit(1);
            }
            prev_char_space = false;
        }
    }//end for loop

    //There was no file after '>', just spaces or null character
    if(output_file_argument_exists == false && c1.outputRedirected == true && char_buffer[0] == '\0' ){
        //parsing error
        fprintf(stderr, "Error: no output file\n");
        c1.number_of_arguments = 0;
        return c1;
    }

     //Add last char_buffer to args or do c1.fd if output redirected
    if(prev_char_space == false){
        if(c1.outputRedirected){
            
            c1.fd = open(char_buffer, O_RDWR | O_CREAT, 0666 | O_TRUNC);
            if(c1.fd == 0){
                fprintf(stderr, "open\n");
                c1.number_of_arguments = 0;
                return c1;
            }
        }else{
            c1.args[current_args_position] = char_buffer;
            current_args_position++;
            number_of_arguments++;
        }
    }

    if(prev_char_space == true && c1.outputRedirected == true){
        //Case where there were extra spaces after output file
        //0644
        c1.fd = open(c1.args[number_of_arguments - 1], O_RDWR | O_CREAT, 0666 | O_TRUNC);
        if(c1.fd == 0){
            fprintf(stderr, "open\n");
            c1.number_of_arguments = 0;
            return c1;
        }

        //Eliminate output file from argument list
        for(int j = 0; j < (int)strlen(char_buffer); j++){
            c1.args[current_args_position][j] = char_buffer[0];
        }
        current_args_position--;
        number_of_arguments--;
    }

    //Add NULL argument to end of argument array
    c1.args[current_args_position] = NULL;
    number_of_arguments++;

    //Have the first argument be the main command
    for(int i = 0; i < (int)strlen(c1.args[0]); i++){
        c1.main_command[i] = c1.args[0][i];
    }
    c1.main_command[(int)strlen(c1.args[0])] = '\0';
    c1.number_of_arguments = number_of_arguments;

    if(number_of_arguments > ARGS_MAX + 1){
        //parsing error
        fprintf(stderr, "Error: too many process arguments\n");
        c1.number_of_arguments = 0;
    }
    return c1;
}//end parse input function

/*
    This function stores the input in a buffer until it hits a pipe or the end of the input. It
    updates the char_buffer and indices accordingly.
*/
void getCharBuffer(char * initial_input, char ** char_buffer, int lineLength, int * j, int * i){
    while(initial_input[*j] != '|' && (*j) < lineLength){
        (*char_buffer)[*i] = initial_input[*j];
        (*char_buffer) = realloc(*char_buffer, (*i)+1);
        if((*char_buffer) == NULL){
            fprintf(stderr, "realloc\n");
            exit(1);
        }
        (*i)++;
        (*j)++;
    }
    return;
}//end function

/*
    This function parses through the entire line from input and creates and
    returns an array of Commands.
*/
struct Command * parseFullLine(char * initial_input, int * number_of_commands){
    char * char_buffer = malloc(sizeof(char) * TOKEN_MAX);
    if(char_buffer == NULL){
        fprintf(stderr, "malloc");
        exit(1);
    }
    struct Command cmds_init[NUM_CMDS_MAX];
    bool parsing_error = false;
    int lineLength = (int)strlen(initial_input);
    int j = 0;  //var to step through initial_input

    //Skip through initial space
    while(initial_input[j] == ' '){
        j++;
    }

    //First non space character is > or | which is an error
    if(initial_input[j] == '>' || initial_input[j] == '|'){
        //Parsing error
        fprintf(stderr, "Error: missing command\n");
        parsing_error = true;
    }

    if(!parsing_error){
        for(int k = 0; k < NUM_CMDS_MAX; ++k){
            int i = 0;  //Var to step through char_buffer
            memset(char_buffer, '\0', sizeof(char)* TOKEN_MAX);
            getCharBuffer(initial_input, &char_buffer, lineLength, &j, &i);

            //Parse the input for a single command
            if(j <= lineLength){
                cmds_init[k] = parseInput(char_buffer);
                if(cmds_init[k].number_of_arguments == 0){
                    //parsing error
                    parsing_error = true;
                }
            } else {
                //No command
                cmds_init[k].number_of_arguments = 0;
            }
            j++;

            if(initial_input[j-1] == '|' && j == lineLength){
                //Parsing error
                fprintf(stderr, "Error: missing command\n");
                parsing_error = true;
            }

            if(j < lineLength){
                if(initial_input[j] == '&'){
                    //Std error redirection
                    cmds_init[k].errorRedirected = true;
                    j++;
                }
                
                //Case when previous character was & and there is nothing after it
                if(initial_input[j] == '\0' && cmds_init[k].errorRedirected == true ){
                    fprintf(stderr, "Error: missing command\n");
                    parsing_error = true;
                }
                //After '|', go to next char that isnt ' '
                while(initial_input[j] == ' '){
                    j++;
                    if(j >= lineLength){
                        //parsing error
                        fprintf(stderr, "Error: missing command\n");
                        parsing_error = true;
                    }
                }
            }else{
                j++;
            }
        }
    }

    //Find actual num of commands
    for(int i = 0; i < NUM_CMDS_MAX; ++i){
        if(cmds_init[i].number_of_arguments > 0){
            ++(*number_of_commands);
        }
    }

    //Create new array with commands of correct size
    struct Command * cmds = malloc((*number_of_commands) * sizeof(struct Command));
    if(cmds == NULL){
        fprintf(stderr, "malloc\n");
        exit(1);
    }
    for(int i = 0; i < (*number_of_commands); ++i){
        cmds[i] = cmds_init[i];
        if(cmds_init[i].outputRedirected && i < (*number_of_commands) - 1){
            fprintf(stderr, "Error: mislocated output redirection\n");
            parsing_error = true;
        }
    }

    free(char_buffer);
    if(parsing_error){
        cmds[0].number_of_arguments = 0;
    }
    return cmds;
}//end function

/*
    This function
*/
void executePipedCmds(struct Command * cmds, int numCmds, char * initial_cmd_copy){
    int prev_pipe[2];
    int cur_pipe[2];
    int * status_array = malloc(sizeof(int) * numCmds);
    if(status_array == NULL){
        fprintf(stderr, "malloc\n");
        exit(1);
    }
    int status = -1;

    for(int i = 0; i < numCmds; ++i){
        if(i < numCmds - 1){
            //not last command
            pipe(cur_pipe);
        }
        if(i == 0){
            //on first command, initialize prev_pipe
            prev_pipe[0] = dup(cur_pipe[0]);
            prev_pipe[1] = dup(cur_pipe[1]);
        }

        int pid = fork();
        if(pid == -1){
            perror("fork");
            exit(1);
        }else if(pid == 0){
            // child
            if(i == 0){
                //first command
                dup2(cur_pipe[1], STDOUT_FILENO);
            }
            else if(i > 0 && i < numCmds - 1){
                //middle command
                dup2(prev_pipe[0], STDIN_FILENO);
                dup2(cur_pipe[1], STDOUT_FILENO);
                if(cmds[i].errorRedirected){
                    //std error redirection
                    dup2(cur_pipe[1], STDERR_FILENO);
                }
            }
            else {
                //last command
                dup2(prev_pipe[0], STDIN_FILENO);
                if(cmds[i].outputRedirected){
                    //output redirection
                    dup2(cmds[i].fd, STDOUT_FILENO);
                }
                if(cmds[i].errorRedirected){
                    //std error redirection
                    dup2(cmds[i].fd, STDERR_FILENO);
                }
                close(cur_pipe[0]);
            }
            close(prev_pipe[0]);
            close(cur_pipe[1]);
            close(prev_pipe[1]);
            //printf("before execvp main command = %s\n", cmds[i].main_command);
            execvp(cmds[i].main_command, cmds[i].args);
        }else{
            // parent
            //printf("IN parent\n")
            close(prev_pipe[0]);
            close(prev_pipe[1]);
            close(cur_pipe[1]);
            if(i == numCmds - 1){
                close(cur_pipe[0]);
            } 
            else{
                prev_pipe[0] = dup(cur_pipe[0]);
                prev_pipe[1] = dup(cur_pipe[1]);
            }
            waitpid(pid, &status, 0);
            status_array[i] = WEXITSTATUS(status);
        }
    }

    fprintf(stderr, "+ completed '%s' ", initial_cmd_copy );
    for(int i = 0; i < numCmds; i++){
        fprintf(stderr, "[%d]", status_array[i]);
    }
    fprintf(stderr, "\n");

    free(status_array);
}//end function

/*
    This function returns the current working directory.
*/
char * getCurrentWorkingDirectory(){
    char * buffer = malloc(sizeof(char) * PATH_MAX);
    if(buffer == NULL){
        fprintf(stderr, "malloc\n");
        exit(1);
    }
    getcwd(buffer, PATH_MAX);
    return buffer;
}//end function

/*
    This function lists the current working directory.
*/
bool pwdFunction(char * initial_cmd_copy){
    char * buffer = getCurrentWorkingDirectory();
    if(buffer == NULL){
        fprintf(stderr, "+ completed '%s' [%d]\n", initial_cmd_copy , 1);
    }else{
        printf("%s\n", buffer);
        fprintf(stderr, "+ completed '%s' [%d]\n", initial_cmd_copy , 0);
    }
    free(buffer);
    return true;
}//end function

/*
    This function changes the current working directory to the
    path given from input.
*/
bool cdFunction(struct Command cmd, char * initial_cmd_copy){
    int exit_status;
    exit_status = chdir(cmd.args[1]); 
    if(exit_status == -1){
        fprintf(stderr, "Error: no such directory\n" );
        exit_status = 1;
    }
    fprintf(stderr, "+ completed '%s' [%d]\n", initial_cmd_copy , exit_status);
    return true;
}//end function

/*
    This functions exits out of the program successfully.
*/
bool exitFunction(char * initial_cmd_copy){
    fprintf(stderr, "Bye...\n");
    fprintf(stderr, "+ completed '%s' [%d]\n", initial_cmd_copy , 0);
    exit(0);
    return true;
}//end function

/*
    This function create a new node for the stack that contains
    the previous directory path and the pointer points to the 
    current stack.
*/
struct Stack * createNewNode(char * oldPath, struct Stack** stack1){
    struct Stack * newNode = (struct Stack*)malloc(sizeof(struct Stack));
    if(newNode == NULL){
        fprintf(stderr, "malloc\n");
        exit(1);
    }
    newNode->directory = oldPath;
    newNode->next = *stack1;
    return newNode;
}//end function

/*
    This function pushes the current directory into the stack and then changes
    directory to the argument stated in the command.
*/
bool pushdFunction(struct Command cmd, char * initial_cmd_copy, struct Stack** stack1){
    //Change current directory to the directory inputted 
    char * oldPath = getCurrentWorkingDirectory();
    int exit_status;
    exit_status = chdir(cmd.args[1]);
    if(exit_status == -1){
        fprintf(stderr, "Error: no such directory\n" );
        exit_status = 1;
    }else{
        //Create and add a new node to the stack
        struct Stack * newNode = createNewNode(oldPath, stack1);
        *stack1 = newNode;
    }
    fprintf(stderr, "+ completed '%s' [%d]\n", initial_cmd_copy , exit_status);
    return true;
}//end function

/*
    This function lists the stack of directories. The first directory
    listed is the current working directory, followed by the directories
    saved previously by pushd fucntion.
*/
bool dirsFunction(char * initial_cmd_copy, struct Stack** stack1){
    //Print the current working directory
    char * current_directory = getCurrentWorkingDirectory();
    printf("%s\n", current_directory);

    //Print the stack of directories
    struct Stack * temp_stack = *stack1;
    while((temp_stack) != NULL){
        printf("%s\n", (temp_stack)->directory );
        temp_stack = (temp_stack)->next;
    }
    free(current_directory);
    fprintf(stderr, "+ completed '%s' [%d]\n", initial_cmd_copy , 0);
    return true;
}//end function

/*
    This function changes the directory to the most recently added 
    directpry in the stack. Ii then pops the latest directory from
    the stack or printsout an error that the directory stack is empty. 
*/
bool popdFunction(char * initial_cmd_copy, struct Stack ** stack1){
    int exit_status = 1;
    //If the directory stack is not empty
    if((*stack1) != NULL){
        //Change the directory to the latest directory from the stack
        exit_status = chdir((*stack1)->directory);
        //Pops the latest directory from the stack
        *stack1 = (*stack1)->next;

        if(exit_status == -1){
            fprintf(stderr, "Error: no such directory\n" );
            exit_status = 1;
        }
    }else{
        fprintf(stderr, "Error: directory stack empty\n");
    }
    fprintf(stderr, "+ completed '%s' [%d]\n", initial_cmd_copy , exit_status);
    return true;
}//end function

/*
    This function calls the appropriate builtin function based of the main_command
    of the Command structure.
*/
bool checkBuiltInCommmands(struct Command cmd, char * initial_cmd_copy, struct Stack** stack1){
    bool builtInCommand = false;
    if(strcmp("pwd", cmd.main_command) == 0 && cmd.number_of_arguments == 2){
        builtInCommand = pwdFunction(initial_cmd_copy);
    }else if(strcmp("cd", cmd.main_command) == 0 && cmd.number_of_arguments == 3){
        builtInCommand = cdFunction(cmd, initial_cmd_copy);
    }else if(strcmp("pushd", cmd.main_command) == 0 && cmd.number_of_arguments == 3){
        builtInCommand = pushdFunction(cmd, initial_cmd_copy, stack1);
    }else if(strcmp("dirs", cmd.main_command) == 0 && cmd.number_of_arguments == 2){
        builtInCommand = dirsFunction(initial_cmd_copy, stack1);
    }else if(strcmp("popd", cmd.main_command) == 0 && cmd.number_of_arguments == 2){
        builtInCommand = popdFunction(initial_cmd_copy, stack1);
    }else if(strcmp("exit", cmd.main_command) == 0 && cmd.number_of_arguments == 2){
        builtInCommand = exitFunction(initial_cmd_copy);
    }
    return builtInCommand;
}//end function

/*
    This function checks if the command is one of the builtin commands and executes 
    it accordingly. Non builtin commands are executed by the fork(), exec(), and 
    waitpid() combination.
*/
void executeCmd(struct Command cmd, char * initial_cmd_copy, struct Stack** stack1){
    bool builtInCommand = checkBuiltInCommmands(cmd, initial_cmd_copy, stack1);

    //Execute fork(), exec(), and waitpid() combination on non builtin command
    if(builtInCommand == false){
        pid_t pid;
        int status;
        pid = fork();
        if (pid == 0) {
            //Child
            if(cmd.outputRedirected){
                dup2(cmd.fd, STDOUT_FILENO);
                if(cmd.errorRedirected){
                    dup2(cmd.fd, STDERR_FILENO);
                }
            }
            execvp(cmd.main_command, cmd.args);
            fprintf(stderr, "Error: command not found\n");
            exit(1);
        }else if (pid > 0){
            //Parent
            waitpid(-1, &status, 0);
            fprintf(stderr, "+ completed '%s' [%d]\n", initial_cmd_copy , WEXITSTATUS(status));
        }else{
            //forking error
            perror("fork");
            exit(1);
        }
    }
}//end function

int main(void){
    char initial_cmd[CMDLINE_MAX];
    int number_of_commands = 0;
    struct Stack * stack1 = NULL;

    while (1){
        char *nl;

        //print prompt
        printf("sshell$ ");
        fflush(stdout);

        //Get command line
        fgets(initial_cmd, CMDLINE_MAX, stdin);

        //Print command line if stdin is not provided by terminal
        if (!isatty(STDIN_FILENO)){
            printf("%s", initial_cmd);
            fflush(stdout);
        }

        //Remove trailing newline from command line
        nl = strchr(initial_cmd, '\n');
        if (nl)
            *nl = '\0';

        //Copy intial_cmd, to be used for output later
        char initial_cmd_copy[CMDLINE_MAX];
        strcpy(initial_cmd_copy, initial_cmd);

        //Parse the command by spaces, pipe, and output redirection
        number_of_commands = 0;
        struct Command * cmds = parseFullLine(initial_cmd, &number_of_commands);
        if(cmds[0].number_of_arguments == 0){
            //Parsing error exit while loop
            continue;
        }

        //Execute the commands accordingly
        if(number_of_commands > 1){
            executePipedCmds(cmds, number_of_commands, initial_cmd_copy);
        }else{
            executeCmd(cmds[0], initial_cmd_copy, &stack1);
        }

        //Free malloc variables
        for(int i = 0; i < number_of_commands; i++){
            for(int j = 0; j < cmds[i].number_of_arguments; j++){
                free(cmds[i].args[j]);
            }
        }
        free(cmds);
    }
    return EXIT_SUCCESS;
}//end main function