#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "interpretor.h"
#include "config.h"
#include "parser.h"
#include "executor.h"


// ===============
//    Execution
// ===============
// ===============

static void execution_init(Execution* exec){
	exec->cmd_tok=NULL;
	exec->input_redir_tok=NULL;
	exec->output_redir_tok=NULL;
	exec->next=NULL;
	exec->argc=0;
	exec->cmd=EXEC_ERR;
	exec->is_append=false;
	exec->is_piped=false;
}

static void execution_print(Execution* exec){
	//cmdname
	dprintf("%.*s", exec->cmd_tok->len, exec->cmd_tok->str);
	//args
	for(int i=0;i<exec->argc;++i){
		dprintf(" %.*s", exec->args[i]->len, exec->args[i]->str);
	}
	//output redir
	if(exec->output_redir_tok){
		if(exec->is_append)
			dprintf(" >> %.*s", exec->output_redir_tok->len, exec->output_redir_tok->str);
		else
			dprintf(" > %.*s", exec->output_redir_tok->len, exec->output_redir_tok->str);
	}
	//input redir
	if(exec->input_redir_tok){
		dprintf(" < %.*s", exec->input_redir_tok->len, exec->input_redir_tok->str);
	}
	//next ptr
	if(exec->next){
		if(exec->is_piped)
			dprintf(" | ");
		else
			dprintf(" && ");
		execution_print(exec->next);
	}
}

// ===============
//     helpers
// ===============

// returns: if token->str==cmd
static bool cmdcmp(const Token* tok, const char* cmd){
	return tok->len==(int)strlen(cmd) && strncmp(tok->str, cmd, tok->len)==0;
}

// returns true if the command is excluded from the shell
static bool is_excluded_command(const Token* tok){
	if(cmdcmp(tok, "fsck")){
		return true;
	}
	return false;
}

// -----forward declaration-----
static bool parse_cmd(const Token*** tokens, int tokens_len, Execution** out_exec);
static int parse_arg(const Token** tokens, const int tokens_len, Execution* exec);
static bool parse_arg_expect(const Token** tokens, const int tokens_len, Execution* exec, int expected_argc);
static bool parse_filename(const Token** tok, int tokens_len, const char* err_msg);
static bool parse_terminate(const Token*** tokens, int tokens_len, Execution* out_exec);
static bool parse_recursive(const Token*** tokens, int tokens_len, Execution* out_exec);

// -----cmd-----
// brief: parses [cmd]
// params:
// - tokens: the tokens to interpret. will advance tokens.
// - tokens_len: length of [tokens]
// - out_exec: sets *out_exec to a new Execution
// returns: true if parsed successfully.
static bool parse_cmd(const Token*** tokens, int tokens_len, Execution** out_exec){
	if(tokens_len<=0){ //eof
		dprintf("[ERROR] a [cmd] expected, but eof\n");
		return false;
	}
	const Token** token_arr=tokens[0];
	if(token_arr[0]->type!=STRING){
		dprintf("[ERROR] a cmdname expected at [%.*s]\n", token_arr[0]->len, token_arr[0]->str);
		return false;
	}
	Execution* exec=(Execution*)malloc(sizeof(Execution));
	execution_init(exec);
	int parsed_argc=parse_arg(token_arr+1, tokens_len-1, exec);
	if(parsed_argc==-1){
		free(exec);
		return false;
	}
	exec->cmd_tok=token_arr[0];
	exec->cmd=EXEC_CMD;
	*out_exec=exec;
	*tokens+=1+parsed_argc; //advance tokens
	return true;
}

// -----argument-----

// brief: parses arguments
// params:
// - tokens: the tokens to interpret
// - tokens_len: length of [tokens]
// - exec: sets exec->args and exec->argc according to parsed arguments.
// returns: number of arguments parsed. -1 if error
static int parse_arg(const Token** tokens, const int tokens_len, Execution* exec){
	int parsed_argc=0;
	for(int i=0;i<tokens_len;++i){
		const Token* token=tokens[i];
		if(token->type!=STRING)
			break;
		if(exec->argc>=EXECUTION_MAX_ARGC){
			dprintf("[ERROR] number of arguments exceeds limit [%d/%d]\n", exec->argc, EXECUTION_MAX_ARGC);
			return -1;
		}
		exec->args[exec->argc++]=token;
		++parsed_argc;
	}
	return parsed_argc;
}

// brief: parses arguments with expected argc
// params:
// - tokens: the tokens to interpret
// - tokens_len: length of [tokens]
// - exec: sets exec->args and exec->argc according to parsed arguments.
// - expected_argc: expected arg count
// returns: true if parsed expected number of args. false otherwise
static bool parse_arg_expect(const Token** tokens, const int tokens_len, Execution* exec, int expected_argc){
	int parsed_argc=parse_arg(tokens, tokens_len, exec);
	return expected_argc==parsed_argc;
}

// -----filename-----
static bool parse_filename(const Token** tokens, int tokens_len, const char* err_msg){
	if(tokens_len==0 || tokens[0]->type!=STRING){
		if(err_msg)
			dprintf("%s", err_msg);
		return false;
	}
	return true;
}

// -----terminate-----
// brief: parses [terminate]
// params:
// - tokens: the tokens to interpret. will advance tokens.
// - tokens_len: length of [tokens]
// - out_exec: might add an output redirection to out_exec
// returns: true if parsed successfully.
static bool parse_terminate(const Token*** tokens, int tokens_len, Execution* out_exec){
	if(tokens_len<=0){ // eof
		return true;
	}
	const Token** token_arr=*tokens;
	int cur_token_idx=1;
	bool success=false;
	switch(token_arr[0]->type){
		case SEMICOLON:
			success=true;
			break;
		case RIGHT: //output redirection
		case APPEND: //append
			success=parse_filename(&token_arr[cur_token_idx], tokens_len-cur_token_idx, "[ERROR] filename expected\n");
			out_exec->output_redir_tok=token_arr[cur_token_idx];
			out_exec->is_append=token_arr[0]->type==APPEND;
			if(success){
				++cur_token_idx;
				//check for ';' or end of line
				if(cur_token_idx<tokens_len && token_arr[cur_token_idx]->type==SEMICOLON)
					++cur_token_idx;
			}
			break;
		default:
			dprintf("[ERROR] expects '>' or '>>' operator (or a ';')\n");
			success=false;
			break;
	}
	*tokens=token_arr+cur_token_idx; //update tokens
	return success;
}

// -----recursive-----
// brief: parses [recursive]
// params:
// - tokens: the tokens to interpret. will advance tokens.
// - tokens_len: length of [tokens]
// - out_exec: will append an Execution after out_exec
// returns: true if parsed successfully.
static bool parse_recursive(const Token*** tokens, int tokens_len, Execution* out_exec){
	if(tokens_len==0) //eof
		return true;
	const Token** end=*tokens+tokens_len;
	if((*tokens)[0]->type!=PIPE){
		dprintf("[ERROR] expect a pipe commandat [%.*s]\n", (*tokens)[0]->len, (*tokens)[0]->str);
		return false;
	}
	(*tokens)++; //advance tokens
	//parse [cmd]
	Execution* exec=NULL;
	if(!parse_cmd(tokens, end-*tokens, &exec))
		return false;
	out_exec->next=exec;
	out_exec->is_piped=true;
	//parse [recursive] or [terminate]
	if(*tokens!=end){
		if((*tokens)[0]->type==PIPE){ //[recursive]
			if(!parse_recursive(tokens, end-*tokens, exec))
				return false;
		} else if(!parse_terminate(tokens, end-*tokens, exec))
			return false;
	}
	return true;
}

#define ERR_RETURN(...) do{\
	dprintf(__VA_ARGS__);\
	free(exec);\
	*out_exec=NULL;\
	return NULL;\
} while(0)

#define ERR_RETURN_NOMSG() do{\
	free(exec);\
	*out_exec=NULL;\
	return NULL;\
} while(0)

// brief: parses the array of tokens into an executable linked list
// params:
// - tokens: the tokens to interpret
// - tokens_len: length of [tokens]
// - out_exec: an Execution linked list (so it should be a pointer to a pointer (caller don't need to allocate memory))
// returns: pointer to the new tokens after parsing this command. NULL if error
static const Token** parse_command(const Token** tokens, int tokens_len, Execution** out_exec){
	// eof
	if(tokens_len==0){
		return tokens;
	}
	// semicolon
	if(tokens[0]->type==SEMICOLON){
		return tokens+1;
	}

	const Token** end_tok=tokens+tokens_len;
	// new Execution
	Execution* exec=(Execution*)malloc(sizeof(Execution));
	execution_init(exec);
	*out_exec=exec;
	// get cmdname
	exec->cmd_tok=*tokens;
	tokens++;
	// =====parse command=====
	// cd
	if(cmdcmp(exec->cmd_tok, "cd")){
		if(!parse_arg_expect(tokens, end_tok-tokens, exec, 1))
			ERR_RETURN("[ERROR] cd command expects one argument\n");
		tokens++;
		exec->cmd=EXEC_CD;
	}
	// exit
	else if(cmdcmp(exec->cmd_tok, "exit")){
		if(tokens!=end_tok){
			if(tokens[0]->type!=SEMICOLON)
				ERR_RETURN("[ERROR] exit command expects no arguments\n");
			++tokens;
		}
		exec->cmd=EXEC_EXIT;
	}
	// fg
	else if(cmdcmp(exec->cmd_tok, "fg")){
		if(!parse_arg_expect(tokens, end_tok-tokens, exec, 1))
			ERR_RETURN("[ERROR] fg command expects one argument\n");
		++tokens;
		exec->cmd=EXEC_FG;
	}
	// jobs
	else if(cmdcmp(exec->cmd_tok, "jobs")){
		if(tokens!=end_tok){
			if(tokens[0]->type!=SEMICOLON)
				ERR_RETURN("[ERROR] jobs command expects no arguments\n");
			++tokens;
		}
		exec->cmd=EXEC_JOBS;
	}
	// excluded commands
	else if(is_excluded_command(exec->cmd_tok)){
		exec->cmd=EXEC_EXCLUDED;
	}
	// external commands
	else if(exec->cmd_tok->type==STRING){
		//parse arguments
		int parsed_argc=parse_arg(tokens, end_tok-tokens, exec);
		if(parsed_argc==-1)
			ERR_RETURN_NOMSG();
		exec->cmd=EXEC_CMD;
		tokens+=parsed_argc;
		//'<'
		if(tokens!=end_tok && tokens[0]->type==LEFT){
			++tokens;
			if(parse_filename(tokens, end_tok-tokens, "[ERROR] < operator expects a filename\n")){
				exec->input_redir_tok=tokens[0];
				++tokens;
			} else
				ERR_RETURN_NOMSG();
		}
		//recursive or terminate
		if(tokens!=end_tok && tokens[0]->type==PIPE){ //[recursive]
			if(!parse_recursive(&tokens, end_tok-tokens, exec))
				ERR_RETURN_NOMSG();
		} else if(!parse_terminate(&tokens, end_tok-tokens, exec)) //[terminate]
			ERR_RETURN_NOMSG();
		//'<'
		if(tokens!=end_tok && tokens[0]->type==LEFT){
			++tokens;
			if(parse_filename(tokens, end_tok-tokens, "[ERROR] < operator expects a filename\n")){
				exec->input_redir_tok=tokens[0];
				++tokens;
			} else
				ERR_RETURN_NOMSG();
		}
	}
	if(tokens!=end_tok){
		dprintf("[ERROR] unexpected token [%.*s]\n", tokens[0]->len, tokens[0]->str);
		return NULL;
	}
	return tokens;
}
// ===============


// brief: given an array of tokens, inteprets the token
// params:
// - tokens: the tokens to interpret
// - len: length of [tokens]
// returns: true if valid command;
InterpretResult interpret(const Token** tokens, int len){
	Execution* execution=NULL;
	for(int i=0;i<len;){
		const Token* tok=tokens[i];
		switch (tok->type){
			case STRING:
				tokens=parse_command(tokens, len, &execution);
				if(tokens==NULL){ // error
					return INTERPRET_INVALID_COMMAND;
				}
				//print commands
				if(execution){
					execution_print(execution);
					dprintf("\n");
				}
				InterpretResult result=execute(execution);
				return result;
				break;
			case SEMICOLON: // end of this command
				++i;
				break;
			default:
				dprintf("[ERROR] expect a command at token [%.*s]\n", tok->len, tok->str);
				return INTERPRET_INVALID_COMMAND;
		}
		break;
	}
	return INTERPRET_SUCCEED;
}
