#pragma once
#include "interpretor.h"
#include <sys/types.h>

struct Execution;

enum {
	JOB_PID_MAX_CNT=128
};

typedef struct Job{
	pid_t pids[JOB_PID_MAX_CNT]; // will be NULL terminated
	char cmd[1024];
} Job;

typedef struct ExecutionInfo{
	// points to an array of pid_t (current running job)
	// NULL terminated
	Job fg_job;
	Job bg_jobs[128];
	int bg_jobs_count;
} ExecutionInfo;

extern ExecutionInfo execution_info;

void job_init(Job* job, Execution* exec);

void executioninfo_init();
void executioninfo_fg_signal();
void executioninfo_fg_sigtstp();
void executioninfo_fg_sigquit();
void executioninfo_print_jobs();
InterpretResult executioninfo_fg_job();
InterpretResult execute(struct Execution* exec);
