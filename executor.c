#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <wait.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include "config.h"
#include "executor.h"
#include "parser.h"

ExecutionInfo execution_info;

//define exit error codes
//used for parent process to get the exit code of forked process
enum{
	EXEC_INVALID_PROGRAM=126,
	EXEC_INVALID_FILE=127
};

// brief: makes params for function execvp
// params:
// - exec: the Execution struct
// - out_argv: a pointer to a char** argv. caller does not need to allocate memory
// returns: true if succeeded
static bool make_execvp_params(const Execution* exec, char*** out_argv){
	char** argv=(char**)malloc(sizeof(char*)*(exec->argc+2));
	argv[exec->argc+1]=NULL; //make sure that argv arr ends with NULL
	//cmdname
	argv[0]=(char*)malloc(sizeof(char)*(exec->cmd_tok->len+1));
	snprintf(argv[0], exec->cmd_tok->len+1, "%.*s", exec->cmd_tok->len, exec->cmd_tok->str);
	for(int i=1;i<exec->argc+1;++i){
		const char* cur_arg=exec->args[i-1]->str;
		int cur_arg_len=exec->args[i-1]->len;
		argv[i]=(char*)malloc(sizeof(char)*(cur_arg_len+1));
		snprintf(argv[i], cur_arg_len+1, "%.*s", cur_arg_len, cur_arg);
	}
	out_argv[0]=argv;
	return true;
}

// brief: free the memory allocated from make_execvp_params
static void free_argv(char** argv){
	for(char** it=argv;*it;++it){
		free(*it);
	}
	free(argv);
}

// recursively execute a linked list of Execution. (will only proceed to the next if piped)
static InterpretResult execution_recursive(Execution* exec, int* prev_pipefd, pid_t* pids){
	if(pids==NULL){
		fprintf(stderr, "[ERROR] pid buffer overflow\n");
		return INTERPRET_ERROR;
	}
	//pipe
	int pipefd[2];
	if(exec->is_piped){
		if(pipe(pipefd)==-1){
			perror("[ERROR] fail to create pipe");
			return INTERPRET_ERROR;
		}
	}
	//fork
	pid_t cur_pid=fork();
	if(cur_pid==-1){
		perror("[ERROR] fail to fork");
		return INTERPRET_ERROR;
	}
	else if(cur_pid==0){ //child process
		if(prev_pipefd){ // handle previous pipe
			dup2(prev_pipefd[0], STDIN_FILENO);
			close(prev_pipefd[0]);
			close(prev_pipefd[1]);
		} else if(exec->input_redir_tok){ //input redirection
			char input_redir_path[512];
			snprintf(input_redir_path, sizeof(input_redir_path)-1, "%.*s", exec->input_redir_tok->len, exec->input_redir_tok->str);
			int fd_in=open(input_redir_path, O_RDONLY);
			if(fd_in<0)
				exit(EXEC_INVALID_FILE);
			dup2(fd_in, STDIN_FILENO);
			close(fd_in);
		}
		if(exec->output_redir_tok){ //output redirection
			char output_redir_path[512];
			snprintf(output_redir_path, sizeof(output_redir_path)-1, "%.*s", exec->output_redir_tok->len, exec->output_redir_tok->str);
			int fd_out;
			//'>' or '>>'
			if(exec->is_append)
				fd_out=open(output_redir_path, O_WRONLY|O_CREAT|O_APPEND, 0644);
			else
				fd_out=open(output_redir_path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if(fd_out<0){
				exit(EXEC_INVALID_FILE);
			}
			dup2(fd_out, STDOUT_FILENO);
			close(fd_out);
		} else if(exec->is_piped){ //pipe
			dup2(pipefd[1], STDOUT_FILENO);
			close(pipefd[0]);
			close(pipefd[1]);
		}
		// copy arguments to argv, which is '\0' terminated
		char** argv=NULL;
		make_execvp_params(exec, &argv);
		const char* file=argv[0];
		// execute
		execvp(file, argv);
		// release resources
		free_argv(argv);
		exit(EXEC_INVALID_PROGRAM);
	} else{ //parent process
		pids[0]=cur_pid;
		// close prev_pipefd
		if(prev_pipefd){
			close(prev_pipefd[0]);
			close(prev_pipefd[1]);
		}
		if(exec->is_piped){
			execution_recursive(exec->next, pipefd, pids+1);
		}
	}
	return INTERPRET_SUCCEED;
}

// brief: executes a linked list of Execution
// returns: true if success
InterpretResult execute(Execution* exec){
	InterpretResult result=INTERPRET_INVALID_PROGRAM;
	switch(exec->cmd){
		case EXEC_CD:{
			const Token* arg=exec->args[0];
			char *path_buffer=(char*)malloc(arg->len+1);
			snprintf(path_buffer, arg->len+1, "%.*s", arg->len, arg->str);
			if(chdir(path_buffer))
				fprintf(stderr, "Error: invalid directory\n");
			result=INTERPRET_SUCCEED;
			break;
		}
		case EXEC_EXIT:
			result=INTERPRET_EXIT;
			break;
		case EXEC_FG:
			break;
		case EXEC_JOBS:
			break;
		case EXEC_CMD:{
			pid_t pids[128];
			memset(pids, 0, sizeof(pids));
			result=execution_recursive(exec, NULL, pids);
			execution_info.fg_jobs=pids;
			// wait for all pids
			for(pid_t* it=pids;*it && result==INTERPRET_SUCCEED;++it){
				int status;
				// if waitpid<0, then it means the user pressed ^C
				if(waitpid(*it, &status, 0)>0 && WIFEXITED(status)){
					int code=WEXITSTATUS(status);
					switch(code){
						case EXEC_INVALID_PROGRAM:
							result=INTERPRET_INVALID_PROGRAM;
							break;
						case EXEC_INVALID_FILE:
							result=INTERPRET_INVALID_FILE;
							break;
						default:
							break;
					}
				} else
					result=INTERPRET_SIGINT;
			}
			execution_info.fg_jobs=NULL;
			break;
		}
		default:
			result=INTERPRET_INVALID_PROGRAM;
			break;
	}
	return result;
}

void executioninfo_fg_sigint(){
	if(execution_info.fg_jobs){
		for(pid_t* it=execution_info.fg_jobs;*it;++it){
			kill(-*it, SIGINT);
			waitpid(*it, NULL, 0);
		}
	}
	execution_info.fg_jobs=NULL;
}

void executioninfo_init(){
	execution_info.fg_jobs=NULL;
}
