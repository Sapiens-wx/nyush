#pragma once

typedef enum {
	STRING, //cmdname, arg, or filename
	// operators
	PIPE, //'|'
	LEFT, //'<'
	RIGHT, //'>'
	APPEND, //'>>'
	// end of cmd
	SEMICOLON, //';'
	ERRTOKEN
} TokenType;

typedef struct Token{
	const char* str;
	int len;
	TokenType type;
} Token;

// brief: parses a given string, and outputs tokens into [out_tokens]
// params:
// - line: the input
// - out_tokens: the array that stores Token*
// - max_token_count: length of the token array
// returns: actual token count if success; -1 if fails
int parse(const char* line, Token** out_tokens, int max_token_count);
