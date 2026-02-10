#pragma once
#include "interpretor.h"
#include <sys/types.h>

struct Execution;

typedef struct ExecutionInfo{
	// points to an array of pid_t (current running job)
	// NULL terminated
	pid_t* fg_jobs;
} ExecutionInfo;

extern ExecutionInfo execution_info;

void executioninfo_init();
void executioninfo_fg_sigint();
InterpretResult execute(struct Execution* exec);
