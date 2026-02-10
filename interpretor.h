#pragma once

// max number of arguments that a command can have
#define EXECUTION_MAX_ARGC 128

// internal command type or command name
typedef enum{
	EXEC_CD,
	EXEC_EXIT,
	EXEC_FG,
	EXEC_JOBS,
	EXEC_CMD, // a command name
	EXEC_ERR
}ExecutionCMD;

//a single command is an execution (separated by the pipe operator)
typedef struct Execution{
	const struct Token* cmd_tok;
	const struct Token* args[EXECUTION_MAX_ARGC];
	//redirection
	const struct Token* input_redir_tok;
	const struct Token* output_redir_tok;
	//next command (is_pipe==true if is piped)
	struct Execution* next;
	//Execution command
	ExecutionCMD cmd;
	int argc;
	//is output_redir_tok append
	bool is_append;
	//if the command after this is piped with this
	bool is_piped;
} Execution;

typedef enum InterpretResult{
	INTERPRET_SUCCEED,
	INTERPRET_INVALID_COMMAND,
	INTERPRET_INVALID_PROGRAM,
	INTERPRET_INVALID_FILE,
	INTERPRET_EXIT,
	INTERPRET_SIGINT,
	INTERPRET_ERROR
} InterpretResult;

InterpretResult interpret(const struct Token** tokens, int len);
