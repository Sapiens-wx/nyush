#define _POSIX_C_SOURCE 200809L
#include <termios.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <wait.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
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

static void tcsetpgrp_if_isatty(pid_t pid){
	// trying to set the foreground process group if is a terminal
	if(isatty(STDIN_FILENO) && tcsetpgrp(STDIN_FILENO, pid)==-1){ //check if it is a terminal
		perror("tcsetpgrp failed");
	}
}

// parses the token's string to a positive integer.
// returns -1 if fails
static int token_to_int(const Token* tok){
	int res=0;
	for(int i=0;i<tok->len;++i){
		int char_val=tok->str[i]-'0';
		if(char_val>=10||char_val<0){
			dprintf("[ERROR] error parsing integer [%.*s]\n", tok->len, tok->str);
			return -1;
		}
		res*=10;
		res+=char_val;
	}
	return res;
}

static InterpretResult wait_pid_get_result(pid_t pid){
	InterpretResult result=INTERPRET_SUCCEED;
	int status;
	// if waitpid<0, then it means the user pressed ^C
	while(1){
		int wpid=waitpid(-pid, &status, WUNTRACED);
		if(wpid==-1){
			if(errno==EINTR) // interrupted by a signal. continue. (this is why we write a loop)
				continue;
			result=INTERPRET_ERROR;
			break;
		}
		// stopped by ^Z
		if(WIFSTOPPED(status)){
			executioninfo_fg_sigtstp();
			result=INTERPRET_SIGNAL;
			break;
		}
		// job terminated by a signal
		if(WIFSIGNALED(status)){
			executioninfo_fg_signal();
			result=INTERPRET_SIGNAL;
			break;
		}
		// exitted normally
		if(WIFEXITED(status)){
			int code=WEXITSTATUS(status);
			switch(code){
				case EXEC_INVALID_PROGRAM:
					result=INTERPRET_INVALID_PROGRAM;
					break;
				case EXEC_INVALID_FILE:
					result=INTERPRET_INVALID_FILE;
					break;
				default:
					result=INTERPRET_SUCCEED;
					break;
			}
			break;
		}
	}
	return result;
}

// gets the executable file from [arg] according to the following rules:
// - absolute path that begins with a slash: return the original
// - relative path e.g., dir/program or ./dir/program: excutes that file
// - base name e.g., program, search UNDER /usr/bin
static bool get_exec_file_from_arg(const char* arg, char* out_file, int size){
	if(arg==NULL)
		return false;
	//absolute path
	if(arg[0]=='/'){
		snprintf(out_file, size, "%s", arg);
	}
	//relative path
	if(strchr(arg, '/')!=NULL){
		snprintf(out_file, size, "%s", arg);
	}
	// base name
	else{
		snprintf(out_file, size, "/usr/bin/%s", arg);
	}
	return true;
}

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
		// set process group id to self
		setpgid(0, 0);
		signal(SIGINT, SIG_DFL);
		signal(SIGTSTP, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
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
		char file[1024];
		if(get_exec_file_from_arg(argv[0], file, sizeof(file))){
			// execute
			execvp(file, argv);
		}
		// release resources
		free_argv(argv);
		exit(EXEC_INVALID_PROGRAM);
	} else{ //parent process
		setpgid(cur_pid, cur_pid);
		pids[0]=cur_pid;
		// close prev_pipefd
		if(prev_pipefd){
			close(prev_pipefd[0]);
			close(prev_pipefd[1]);
		} else{ // if this is the first command (if several commands are piped)
			tcsetpgrp_if_isatty(cur_pid);
		}
		if(exec->is_piped){
			execution_recursive(exec->next, pipefd, pids+1);
		}
	}
	return INTERPRET_SUCCEED;
}

// -----job struct-----

void job_init(Job* job, Execution* exec){
	if(exec==NULL)
		job->cmd[0]=0;
	else
		snprintf(job->cmd, sizeof(job->cmd)-1, "%s", exec->cmd_tok->str);
	memset(job->pids, 0, sizeof(job->pids));
}

// -----for the jobs command-----

void executioninfo_fg_signal(){
	job_init(&execution_info.fg_job, NULL);
}

void executioninfo_fg_sigtstp(){
	execution_info.bg_jobs[execution_info.bg_jobs_count++]=execution_info.fg_job;
	job_init(&execution_info.fg_job, NULL);
}

void executioninfo_print_jobs(){
	for(int i=0;i<execution_info.bg_jobs_count;++i){
		printf("[%d] %s\n", i+1, execution_info.bg_jobs[i].cmd);
	}
}

// used by the 'fg' command.
// puts a background job to the foreground
InterpretResult executioninfo_fg_job(int idx){
	if(idx<0 || idx>=execution_info.bg_jobs_count){
		dprintf("[ERROR] job index out of bounds [%d/%d]\n", idx, execution_info.bg_jobs_count);
		return INTERPRET_ERROR;
	}
	execution_info.fg_job=execution_info.bg_jobs[idx];
	// remove the job from the background
	--execution_info.bg_jobs_count;
	for(int i=idx;i<execution_info.bg_jobs_count;++i){
		execution_info.bg_jobs[i]=execution_info.bg_jobs[i+1];
	}
	// resume the job
	if(execution_info.fg_job.pids[0]==0)
		dprintf("[ERROR] resuming a job but its pids is empty\n");
	tcsetpgrp_if_isatty(execution_info.fg_job.pids[0]);
	for(pid_t* it=execution_info.fg_job.pids;*it;++it){
		kill(*it, SIGCONT);
	}
	// wait for all pids
	InterpretResult result=INTERPRET_SUCCEED;
	for(pid_t* it=execution_info.fg_job.pids;*it && result==INTERPRET_SUCCEED;++it){
		result=wait_pid_get_result(*it);
	}
	return result;
}

void executioninfo_init(){
	job_init(&execution_info.fg_job, NULL);
	execution_info.bg_jobs_count=0;
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
			if(execution_info.bg_jobs_count>0){
				fprintf(stderr, "Error: there are suspended jobs\n");
				result=INTERPRET_SUCCEED;
			} else{
				result=INTERPRET_EXIT;
			}
			break;
		case EXEC_FG:{
			int fg_idx=token_to_int(exec->args[0])-1;
			if((result=executioninfo_fg_job(fg_idx))==INTERPRET_ERROR) {
				fprintf(stderr, "Error: invalid job\n");
				result=INTERPRET_SUCCEED;
			}
			break;
		}
		case EXEC_JOBS:
			executioninfo_print_jobs();
			result=INTERPRET_SUCCEED;
			break;
		case EXEC_CMD:{
			// update fg_jobs
			job_init(&execution_info.fg_job, exec);
			result=execution_recursive(exec, NULL, execution_info.fg_job.pids);
			// wait for all pids
			for(pid_t* it=execution_info.fg_job.pids;*it && result==INTERPRET_SUCCEED;++it){
				result=wait_pid_get_result(*it);
			}
			job_init(&execution_info.fg_job, NULL);
			break;
		}
		default:
			result=INTERPRET_INVALID_PROGRAM;
			break;
	}
	tcsetpgrp_if_isatty(getpgrp()); // set foreground group
	return result;
}
