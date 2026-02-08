#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <wait.h>
#include <stdio.h>
#include "config.h"
#include "executor.h"
#include "parser.h"

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

// brief: wrapper method for execvp().
// 		  fork and then execvp. will block until forked process terminated
// returns: true if succeed
static bool execvp_wrapper(const char* file, char *const argv[]){
	pid_t pid=fork();
	if(pid==0){ //child process
		execvp(file, argv);
		exit(127);
	} else{ //parent. waits for child process to terminate
		int status;
		waitpid(pid, &status, 0);
		if(WIFEXITED(status)){
			int code=WEXITSTATUS(status);
			return code!=127;
		}
	}
	return false;
}

// brief: executes an Execution
// params:
// - exec: the Execution struct to execute
// - prev: the Execution struct such that prev->next==exec
// returns: true if success
static bool execute_single(Execution* exec, Execution* prev){
	if(prev)
		dprintf("this line is used to avoid unused param warning\n");
	bool success=false;
	switch(exec->cmd){
		case EXEC_CD:
			break;
		case EXEC_EXIT:
			exit(0);
			break;
		case EXEC_FG:
			break;
		case EXEC_JOBS:
			break;
		case EXEC_CMD:{
			char** argv=NULL;
			make_execvp_params(exec, &argv);
			if(execvp_wrapper(argv[0], argv))
				success=true;
			free_argv(argv);
			break;
					  }
		default:
			success=false;
			break;
	}
	return success;
}

// brief: executes a linked list of Execution
// returns: true if success
bool execute(Execution* exec){
	Execution* prev=NULL;
	for(Execution* cur=exec;cur;cur=cur->next){
		if(!execute_single(cur, prev))
			return false;
		prev=cur;
	}
	return true;
}
