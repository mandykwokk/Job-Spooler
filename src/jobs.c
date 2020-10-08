/*
 * Job manager for "jobber".
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "jobber.h"
#include "task.h"
#include "helper.h"


int jobs_init(void) {//malloc?
    sf_set_readline_signal_hook(&hook_func);
    return 0;
}

void jobs_fini(void) {//free?
    for(int i=0;i<MAX_JOBS;i++){
        if(job_get_taskspec(i)!=NULL){
            if(job_get_status(i)!=COMPLETED&&job_get_status(i)!=ABORTED)
                job_cancel(i);
        }
    }//finish status changing
    for(int i=0;i<MAX_JOBS;i++){
        if(job_get_taskspec(i)!=NULL)
            job_expunge(i);
    }
}

int jobs_set_enabled(int val) {
    int oldflag = flag;
    flag = val;
    return oldflag;
}

int jobs_get_enabled() {
    return flag;
}

int job_create(char *command) {//malloc?
    int i;
    for(i=0;i<MAX_JOBS;i++){
        if(jobs_table[i].job_spec==0){
            jobs_table[i].job_spec = command;
            jobs_table[i].job_pgid = 0;//getpid();
            break;
        }
    }
    if(i==MAX_JOBS&&jobs_table[i-1].job_spec!=0)
        return -1;
    TASK* t = parse_task(&command);
    if(t!=NULL){
        jobs_table[i].job_task = t;
        return i;
    }
    return -1;
}

int job_expunge(int jobid) {
    if((jobid<0||jobid>=MAX_JOBS)||(jobs_table[jobid].job_spec==0)||(jobs_table[jobid].job_status!=COMPLETED&&jobs_table[jobid].job_status!=ABORTED)){
        printf("Error: expunge\n");
        return -1;
    }
    //free?
    jobs_table[jobid].job_spec = 0;
    jobs_table[jobid].job_status = NEW;
    jobs_table[jobid].job_pgid = 0;
    jobs_table[jobid].job_result = 0;
    jobs_table[jobid].canceled = 0;
    jobs_table[jobid].job_task = NULL;
    sf_job_expunge(jobid);
    return 0;
}

int job_cancel(int jobid) {
    if((jobid<0||jobid>=MAX_JOBS)||(job_get_taskspec(jobid)==NULL)||(job_get_status(jobid)==COMPLETED||job_get_status(jobid)==ABORTED)){
        printf("Error: cancel\n");
        return -1;
    }
    if(jobs_table[jobid].job_status==WAITING){
        jobs_table[jobid].job_status = ABORTED;
        sf_job_status_change(jobid, WAITING, ABORTED);
        return 0;
    }
    else if(jobs_table[jobid].job_status==RUNNING||jobs_table[jobid].job_status==PAUSED){//running paused
        JOB_STATUS old_status = jobs_table[jobid].job_status;
        sigset_t mask_child, prev_one;
        if(sigemptyset(&mask_child) < 0)
            return -1;
        if(sigaddset(&mask_child, SIGCHLD) < 0)
            return -1;
        sigprocmask(SIG_BLOCK, &mask_child, &prev_one);
        if(killpg(jobs_table[jobid].job_pgid,SIGKILL)==-1)
            return -1;
        jobs_table[jobid].job_status = CANCELED;
        sf_job_status_change(jobid,old_status,CANCELED);
        jobs_table[jobid].canceled = 1;
        sigprocmask(SIG_SETMASK, &prev_one, NULL);
        return 0;
    }
    return -1;
}

int job_pause(int jobid) {
    if((jobid<0||jobid>=MAX_JOBS)||(job_get_taskspec(jobid)==NULL)||(job_get_status(jobid)!=RUNNING)){
        printf("Error: pause\n");
        return -1;
    }
    sigset_t mask_child, prev_one;
    if(sigemptyset(&mask_child) < 0) return -1;
    if(sigaddset(&mask_child, SIGCHLD) < 0) return -1;
    sigprocmask(SIG_BLOCK, &mask_child, &prev_one);
    if(killpg(jobs_table[jobid].job_pgid,SIGSTOP)==-1) return -1;
    jobs_table[jobid].job_status = PAUSED;
    sf_job_status_change(jobid,RUNNING,PAUSED);
    sf_job_pause(jobid,jobs_table[jobid].job_pgid);
    sigprocmask(SIG_SETMASK, &prev_one, NULL);
    return 0;
}

int job_resume(int jobid) {
    if((jobid<0||jobid>=MAX_JOBS)||(job_get_taskspec(jobid)==NULL)||(job_get_status(jobid)!=PAUSED)){
        printf("Error: resume\n");
        return -1;
    }
    sigset_t mask_child, prev_one;
    if(sigemptyset(&mask_child) < 0) return -1;
    if(sigaddset(&mask_child, SIGCHLD) < 0) return -1;
    sigprocmask(SIG_BLOCK, &mask_child, &prev_one);
    if(killpg(jobs_table[jobid].job_pgid,SIGCONT)==-1) return -1;
    jobs_table[jobid].job_status = RUNNING;
    sf_job_status_change(jobid,PAUSED,RUNNING);
    sf_job_resume(jobid,jobs_table[jobid].job_pgid);
    sigprocmask(SIG_SETMASK, &prev_one, NULL);
    return 0;
}

int job_get_pgid(int jobid) {
    if(jobid>=0&&jobid<MAX_JOBS){
        if(job_get_status(jobid)==-1||(job_get_status(jobid)!=RUNNING&&job_get_status(jobid)!=PAUSED&&job_get_status(jobid)!=CANCELED))
            return -1;
        if(jobs_table[jobid].job_spec!=0)
            return jobs_table[jobid].job_pgid;
    }
    return -1;
}

JOB_STATUS job_get_status(int jobid) {
    if(jobid>=0&&jobid<MAX_JOBS){
        if(jobs_table[jobid].job_spec!=0)
            return jobs_table[jobid].job_status;
    }
    return -1;
}

int job_get_result(int jobid) {
    if(jobid>=0&&jobid<MAX_JOBS){
        if(job_get_status(jobid)==-1||(job_get_status(jobid)!=COMPLETED))
            return -1;
        if(jobs_table[jobid].job_spec!=0)
            return jobs_table[jobid].job_result;
    }
    return -1;
}

int job_was_canceled(int jobid) {
    if(jobs_table[jobid].canceled==1)
        return 1;
    return 0;
}

char *job_get_taskspec(int jobid) {
    if(jobid>=0&&jobid<MAX_JOBS){
        if(jobs_table[jobid].job_spec!=0)
            return jobs_table[jobid].job_spec;
    }
    return NULL;
}