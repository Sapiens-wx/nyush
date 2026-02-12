#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <wait.h>
#include <fcntl.h>
#include "parser.h"
#include "interpretor.h"
#include "executor.h"
#include "workdir.h"

// -----signal handler-----

static void register_signal_handlers(){
	//struct sigaction sa = {0};

    //sa.sa_handler = signal_handler;
    //sigaction(SIGCHLD, &sa, NULL);

	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
}

int main(){
	executioninfo_init();
	register_signal_handlers();
	//loop
	Token* token_buffer[1024];
	char buffer[1024]="\0";
	bool is_running=true;
	while(is_running){
		cwd_update();
		if(feof(stdin))
			break;
		printf("[nyush %s]$ ", cwd_get());
		fflush(stdout);
		buffer[0]='\0';
		fgets(buffer, sizeof(buffer), stdin);
		buffer[strcspn(buffer, "\n")]='\0'; // get rid of the \n char
		int tokens_len=parse(buffer, token_buffer, sizeof(token_buffer));
		if(tokens_len==-1){ // error
			fprintf(stderr, "Error: invalid command\n");
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
				case INTERPRET_SIGNAL:
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
