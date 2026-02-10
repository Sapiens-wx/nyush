#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "parser.h"
#include "interpretor.h"
#include "executor.h"

static char cwd[1024];

void update_cwd(){
	//get cwd
	if(getcwd(cwd, sizeof(cwd))==NULL){
		printf("error getting current working directory!\n");
	}
}

// -----signal handler-----
static void handle_sigint(int sig) {
	sig++;
	executioninfo_fg_sigint();
}

static void handle_sigquit(int sig) {
	sig++;
    write(STDOUT_FILENO, "Caught SIGQUIT\n", 15);
}

static void handle_sigtstp(int sig) {
	sig++;
    write(STDOUT_FILENO, "Caught SIGTSTP\n", 15);
}

static void register_signal_handlers(){
	struct sigaction sa = {0};

    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = handle_sigquit;
    sigaction(SIGQUIT, &sa, NULL);

    sa.sa_handler = handle_sigtstp;
    sigaction(SIGTSTP, &sa, NULL);
}

int main(){
	executioninfo_init();
	register_signal_handlers();
	//loop
	Token* token_buffer[1024];
	char buffer[1024]="\0";
	bool is_running=true;
	while(is_running){
		update_cwd();
		printf("[nyush %s]$ ", cwd);
		fflush(stdout);
		buffer[0]='\0';
		fgets(buffer, sizeof(buffer), stdin);
		buffer[strcspn(buffer, "\n")]='\0'; // get rid of the \n char
		int tokens_len=parse(buffer, token_buffer, sizeof(token_buffer));
		if(tokens_len==-1){ // error
			printf("Error: invalid command\n");
		} else{
			switch(interpret((const Token**)token_buffer, tokens_len)){
				case INTERPRET_INVALID_COMMAND:
					fprintf(stderr, "Error: invalid command\n");
					break;
				case INTERPRET_INVALID_PROGRAM:
					fprintf(stderr, "Error: invalid program\n");
					break;
				case INTERPRET_INVALID_FILE:
					fprintf(stderr, "Error: invalid file\n");
					break;
				case INTERPRET_EXIT:
					is_running=false;
					break;
				case INTERPRET_SIGINT:
					printf("\n");
					break;
				case INTERPRET_ERROR:
					fprintf(stderr, "[ERROR]: internal error\n");
					break;
				case INTERPRET_SUCCEED:
					break;
			}
		}
	}
	return 0;
}
