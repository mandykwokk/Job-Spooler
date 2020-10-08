#include <signal.h>
#include <stdio.h>

typedef struct JOB {
    char* job_spec;
    JOB_STATUS job_status;
    pid_t job_pgid;
    int job_result;
    TASK* job_task;
    int canceled;
} JOB;

JOB jobs_table[MAX_JOBS];

volatile sig_atomic_t flag;

volatile sig_atomic_t init_completed;

static char *trim(char *str, char toTrim);

void job_set_status(int jobid, JOB_STATUS status);

int smallest_runnable();

void jobs_run();

int running;

int exist_running_job();

int hook_func();