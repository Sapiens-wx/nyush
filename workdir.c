#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "workdir.h"

static char cwd[1024];

const char* cwd_get(){
	return cwd;
}

void cwd_update(){
	char buf[1024];
	if(getcwd(buf, sizeof(buf))==NULL)
		fprintf(stderr, "error getting current working directory!\n");
	
	char* last_dir=strrchr(buf, '/');
	if(strlen(buf)>1){ // is in a normal directory
		snprintf(cwd, sizeof(cwd), "%s", last_dir+1);
	} else if(last_dir){ // is a root directory ('/')
		snprintf(cwd, sizeof(cwd), "%s", buf);
	} else{
		snprintf(cwd, sizeof(cwd), "%s", buf);
	}
}
