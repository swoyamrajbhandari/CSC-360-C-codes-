#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

// Structure to represent a background job
struct BackgroundProcess {
    pid_t pid;            // Process ID of the background job
    char command[1024];   // The command and its arguments
    struct BackgroundProcess* next; // Pointer to the next background job
};
struct BackgroundProcess* bg_head = NULL;

void print_prompt();
void handle_cd_command(char* tokens[], int token_count);
void handle_bg_command(char* tokens[], int token_count);
void handle_other_command(char* tokens[]);
void add_background_process(pid_t pid, char** command);
void remove_terminated_process();
void print_termination_message(const char* command, pid_t pid);
void print_background_list();

void print_prompt() {
    char hostname[1024];
    gethostname(hostname, sizeof(hostname));

    char cwd[1024];
    getcwd(cwd, sizeof(cwd));

    char* username = getlogin();

    printf("%s@%s: %s >", username, hostname, cwd);
}

void add_background_process(pid_t pid, char** command) {
    struct BackgroundProcess* process = (struct BackgroundProcess*)malloc(sizeof(struct BackgroundProcess));
    process->pid = pid;

    strncpy(process->command, command[0], sizeof(process->command));
    for (int i = 1; command[i] != 0; i++ ){
        strncat(process->command, " ", sizeof(process->command) - strlen(process->command) - 1);
        strncat(process->command, command[i], sizeof(process->command) - strlen(process->command) - 1);
    } 
    process->next = NULL;

    if (bg_head == NULL) {     // Add the job to the linked list
        bg_head = process;
    } else {
        struct BackgroundProcess* current = bg_head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = process;
    }
}

void remove_terminated_process() {
    struct BackgroundProcess* current = bg_head;
    struct BackgroundProcess* prev = NULL;

    while (current != NULL) {
        int status;
        pid_t result = waitpid(current->pid, &status, WNOHANG);

        if (result == -1) {
            perror("waitpid");

        } else if (result == 0) {       // The process is still running

            prev = current;
            current = current->next;
        } else {           // The process has terminated
            
            print_termination_message(current->command, current->pid);
            if (prev == NULL) {          // Remove the first job in the list
               
                bg_head = current->next;
                free(current);
                current = bg_head;
            } else {
                prev->next = current->next;
                free(current);
                current = prev->next;
            }
        }
    }
}

void print_termination_message(const char* command, pid_t pid) {

    char cwd[1024];
    getcwd(cwd, sizeof(cwd));

    printf("%d: %s/%s has terminated\n", pid, cwd, command);
}


void print_background_list() {
    struct BackgroundProcess* current = bg_head;
    int count = 0;

    while (current != NULL) {
        printf("%d: %s\n", current->pid, current->command);
        current = current->next;
        count++;
    }

    printf("Total Background jobs: %d\n", count);
}

void handle_cd_command(char* tokens[], int token_count) {

    if (token_count == 1 || (token_count == 2 && strcmp(tokens[1], "~") == 0)) {  // If no argument or "~" is provided, change to the home directory
            chdir(getenv("HOME"));

        } else if (strcmp(tokens[1], ".") == 0) {     // If "." is provided, do nothing (stay in the current directory) 
   
        } else if (strcmp(tokens[1], "..") == 0) {   // If ".." is provided, move to the parent directory
            chdir("..");
        } else {
            if (chdir(tokens[1]) != 0) {      // Change to the specified directory
                perror("chdir");
            }
        }

}

void handle_bg_command(char* tokens[], int token_count){

    if (token_count >= 2) {      // Remove the "bg" token
        
        for (int i = 0; i < token_count; i++) {
            tokens[i] = tokens[i + 1];
        }
        token_count--;

        pid_t pid = fork();
        if (pid == 0) {        // Child process
            
            if (execvp(tokens[0], tokens) == -1) {
                perror("execvp");
                exit(1);
            }
        } else if (pid < 0) {
            perror("fork");
    
        } else {       // Parent process
            
            add_background_process(pid, tokens); 
            printf("Background job started with PID: %d\n", pid);
        }
    
    }

}

void handle_other_command(char* tokens[]){

    pid_t pid = fork();

    if (pid == 0) {     // Child process
        
        if (execvp(tokens[0], tokens) == -1) {
            perror("execvp");
            exit(1);
        }
    } else if (pid < 0) {
        perror("fork");
    
    } else {         // Parent process
      
        int status;
        waitpid(pid, &status, 0);    //wait for the child process to terminate.

    }

}

int main(){
    while(1){

        remove_terminated_process(); 

        print_prompt();
        int background_execution = 0;

        char input[1034];
        fgets(input, sizeof(input), stdin);

        // Remove newline character from input
        int input_len = strlen(input);
        if (input_len > 0 && input[input_len - 1] == '\n') {
            input[input_len - 1] = '\0';
        }

        char* tokens[100];
        int token_count = 0;

        char* token = strtok(input, " ");
        while (token != NULL) {
                tokens[token_count] = token;
                token_count++;
                token = strtok(NULL, " ");
        }
        tokens[token_count] = NULL;

        if (token_count > 0) {
            if (strcmp(tokens[0], "cd") == 0) {          //handle cd commands
                handle_cd_command(tokens, token_count);

            } else if (strcmp(tokens[0], "exit") == 0){   //handle exit command
                exit(0);
        
            } else if (strcmp(tokens[0], "bglist") == 0) {   // Handle the "bglist" command
                print_background_list();

            } else if (strcmp(tokens[0], "bg") == 0) {     // Handle the "bg" command
                handle_bg_command(tokens, token_count);

            } else {           // Handle other commands...
                handle_other_command(tokens);
            }
        
        }
    }
    return 0;
}