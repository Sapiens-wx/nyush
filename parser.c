#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "config.h"

const char* STRING_PATTERN_COMPLEMENT = " \t><|*!`'\"";

// ==================
//    helper funcs
// ==================

// brief: given a string, gets a char* tok that represents the first token it encounters
// params:
// - line: the line to parse
// - tok [out]: sets *tok to point at the first token
// - len [out]: sets *len to the length of the token
//              *len==0 if end of line
//              *len==-1 if has an error
// returns: a pointer pointing to the [line] after parsing the first token
static const char* get_token(const char* line, const char** tok, int* len){
	//skip whitespaces
	line+=strspn(line, " \t");
	//end of line
	if(*line=='\0'){
		*len=0;
		return line;
	}
	//check for invalid chars
	if(strspn(line, "*!`'\"")){
		*len=-1;
		return line;
	}
	//operators: | , > , >> , < , ;
	if((*len=strspn(line, "|<>;"))>0){
		*tok=line;
		if(*len>1){
			if(line[0]!='>')
				*len=1;
			else if(line[0]=='>' && line[1]=='>') // '>>' operator
				*len=2;
			else
				*len=1;
		}
		return line + *len;
	}
	//a string (either a cmdname, an arg, or a filename)
	if((*len=strcspn(line, STRING_PATTERN_COMPLEMENT))>0){
		*tok=line;
		return line + *len;
	}
	//unknown error
	*len=-1;
	return line;
}

// brief: given a string, parses and returns a token
// params:
// - tok: the string token
// - len: length of the token
// returns: a token. token->type==ERRTOKEN if error
static Token* parse_token(const char* tok, int len){
	Token* ret=(Token*)malloc(sizeof(Token));
	ret->str=tok;
	ret->len=len;
	ret->type=ERRTOKEN;
	if(len==0)
		return ret;
	//set token type
	switch(tok[0]){
		case ';':
			ret->type=SEMICOLON;
			break;
		case '|':
			ret->type=PIPE;
			break;
		case '<':
			ret->type=LEFT;
			break;
		case '>':
			if(len==1)
				ret->type=RIGHT;
			else if(len==2 && tok[1]=='>')
				ret->type=APPEND;
			break;
		default:
			ret->type=STRING;
			break;
	}
	return ret;
}

// ==================

// brief: parses a given string, and outputs tokens into [out_tokens]
// params:
// - line: the input
// - out_tokens: the array that stores Token*
// - max_token_count: length of the token array
// returns: actual token count if success; -1 if fails
int parse(const char* line, Token** out_tokens, int max_token_count){
	int token_count=0;
	for(const char* it=line;*it;){
		const char* tok=NULL;
		int len=0;
		it=get_token(it, &tok, &len);
		if(len==0){ //end of line
			break;
		} else if(len==-1){ //error
			dprintf("[ERROR] when getting token at position [%d] of line [%s]\n", (int)(it-line), line);
			return -1;
		} else if(token_count>=max_token_count){ //buffer overflow
			dprintf("[WARNING] parse reaches max_token_count\n");
			return -1;
		}
		else{
			Token* token=parse_token(tok, len);
			out_tokens[token_count]=token;
			token_count++;
			if(token->type==ERRTOKEN){
				dprintf("[ERROR] parsing token [%.*s]\n", token->len, token->str);
				return -1;
			}
		}
	}
	return token_count;
}
