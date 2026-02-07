#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include "parser.h"
#include "interpretor.h"

static char cwd[1024];

void print_tokens(Token** tokens, int n){
	for(int i=0;i<n;++i){
		printf("[%.*s] ", tokens[i]->len, tokens[i]->str);
	}
	printf("\n");
}

int main(){
	//get cwd
	if(getcwd(cwd, sizeof(cwd))==NULL){
		printf("error getting current working directory!\n");
		return 1;
	}
	//loop
	Token* token_buffer[1024];
	char buffer[1024]="\0";
	bool is_running=true;
	while(is_running){
		printf("[nyush %s]$ ", cwd);
		fflush(stdout);
		buffer[0]='\0';
		fgets(buffer, sizeof(buffer), stdin);
		buffer[strcspn(buffer, "\n")]='\0'; // get rid of the \n char
		int tokens_len=parse(buffer, token_buffer, sizeof(token_buffer));
		if(tokens_len==-1){ // error
		} else{
			if(!interpret((const Token**)token_buffer, tokens_len)){
				printf("Error: invalid command\n");
			}
		}
	}
	return 0;
}
