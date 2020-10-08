#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "task.h"
#include "jobber.h"
#include "helper.h"

/*
char *trim(char *str, char toTrim)
{
  char *end;
  while((unsigned char)*str==toTrim) str++;
  if(*str == 0)
    return str;
  end = str + strlen(str) - 1;
  while(end > str && ((unsigned char)*end==toTrim)) end--;
  end[1] = '\0';
  return str;
}
*/
volatile sig_atomic_t done;
int ccount = 0;
int pids[MAX_RUNNERS];
int pcount = 0;

void pipeline_handler(int sig)
{
    int olderrno = errno;
    errno = olderrno;
}

void cmd_handler(int sig)
{
    int olderrno = errno;
    errno = olderrno;
}

void sigchld_handler(int sig)
{
    int olderrno = errno;
    sigset_t mask_child, prev_one;
    sigemptyset(&mask_child);
    sigaddset(&mask_child, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask_child, &prev_one);
    int status;
    pid_t pid;
    while(ccount>0&&(pid = waitpid(-1,&status,WNOHANG))!=0){
        if(pid<0){
            ;//printf("wait error\n");
        }
        --ccount;
        int job_index = 0;
        for(int i=0;i<MAX_JOBS;i++){
            if(jobs_table[i].job_pgid == pid){
                job_index = i;
                break;
            }
        }
        //printf("Child exited because of signal %d\n", WEXITSTATUS(status));
        running--;
        if(WIFEXITED(status)!=0){
            jobs_table[job_index].job_result = WEXITSTATUS(status);
            JOB_STATUS XX =  jobs_table[job_index].job_status;
            jobs_table[job_index].job_status = COMPLETED;
            jobs_table[job_index].job_pgid = 0;
            sf_job_end(job_index, pid, WEXITSTATUS(status));
            sf_job_status_change(job_index, XX, COMPLETED);
            //printf("job %d pid: %d reaped and exited normally\n", job_index, pid);
        }
        else{
            jobs_table[job_index].job_result = -1;
            JOB_STATUS XX =  jobs_table[job_index].job_status;
            jobs_table[job_index].job_status = ABORTED;
            jobs_table[job_index].job_pgid = 0;
            sf_job_end(job_index, pid, status);
            sf_job_status_change(job_index, XX, ABORTED);
            //printf("job %d pid: %d reaped and exited abnormally\n",job_index, pid);
        }
        int newfork;
        if(running<MAX_RUNNERS&&(newfork = smallest_runnable())!=-1){
            int pid;
            running++;
            ccount++;
            jobs_table[newfork].job_status = RUNNING;
            if((pid = fork())==0){//child
                //copy begin
                TASK* task = jobs_table[newfork].job_task;
                    signal(SIGCHLD, pipeline_handler);
                    PIPELINE_LIST* pipelist_ptr = task->pipelines->rest;
                    PIPELINE* current_pipeline = task->pipelines->first;
                    //pcount = 0;
                    while(1){
                        int pgid = getpid();//runner process
                        setpgid(getpid(), pgid);
                        //pcount++;
                        if(fork()==0){//pipeline process
                            //printf("pipeline pid: %d pgid: %d\n", getpid(), pgid);
                            setpgid(getpid(), pgid);
                            //start command process
                            COMMAND_LIST* cmdlist_ptr = current_pipeline->commands->rest;
                            COMMAND* current_cmd= current_pipeline->commands->first;
                            int command_count = 0;
                            if(current_cmd!=NULL)
                                command_count++;
                            COMMAND_LIST* count_ptr = cmdlist_ptr;
                            while(count_ptr!=NULL){
                                command_count++;
                                count_ptr = count_ptr->rest;
                            }
                            pcount = 0;//number of command process, in pipe process
                            int first_command = 1;
                            int last_command = 0;
                            int command_order = -1;
                            int fds[command_count][2];
                            for(int i=0;i<command_count;i++){
                                pipe(fds[i]);
                            }
                            while(1){
                                pcount++;
                                signal(SIGCHLD, cmd_handler);
                                command_order++;
                                if(fork()==0){//fork command process
                                    //printf("command: %s pid: %d pgid: %d\n",current_cmd->words->first, getpid(), pgid);
                                    if(command_count==1){
                                        if(current_pipeline->input_path!=NULL){
                                            int in = open(current_pipeline->input_path, O_RDONLY);
                                            dup2(in,0);
                                            close(in);
                                        }
                                        if(current_pipeline->output_path!=NULL){
                                            int out = open(current_pipeline->output_path, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
                                            dup2(out,1);
                                            close(out);
                                        }
                                    }
                                    else{//multiple commands
                                        if(first_command){
                                            //printf("first command\n");
                                            close(fds[command_order][0]);
                                            dup2(fds[command_order][1], 1);
                                            close(fds[command_order][1]);
                                        }
                                        else if(last_command){
                                            //printf("last_command\n" );
                                            int in = open(current_pipeline->input_path, O_RDONLY);
                                            int out = open(current_pipeline->output_path, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
                                            close(fds[command_order][1]);
                                            dup2(in, STDIN_FILENO);
                                            dup2(out, STDOUT_FILENO);
                                            if(in!=STDIN_FILENO) close(in);
                                            if(out!=STDOUT_FILENO) close(out);
                                            dup2(fds[command_order-1][0], 0);
                                            close(fds[command_order-1][0]);
                                        }
                                        else{
                                            //printf("middle command\n");
                                            close(fds[command_order][0]);
                                            dup2(fds[command_order][1], 1);
                                            close(fds[command_order][1]);
                                            dup2(fds[command_order-1][0], 0);
                                            close(fds[command_order-1][0]);
                                        }
                                    }
                                    setpgid(getpid(), pgid);
                                    int command_arg_length = 0;
                                    WORD_LIST* rest_ptr = current_cmd->words->rest;
                                    while(rest_ptr!=NULL){
                                        command_arg_length++;
                                        rest_ptr = rest_ptr->rest;
                                    }
                                    char* args[command_arg_length+2];
                                    args[0] = current_cmd->words->first;
                                    rest_ptr = current_cmd->words->rest;
                                    for(int i=0;i<command_arg_length;i++){
                                        args[i+1] = rest_ptr->first;
                                        rest_ptr = rest_ptr->rest;
                                    }
                                    args[command_arg_length+1] = NULL;
                                    if(execvp(current_cmd->words->first, args)==-1){
                                        perror("execvp failed");
                                        abort();//aborted command
                                    }
                                    exit(0);//end command
                                }
                                close(fds[command_order][1]);
                                if(!first_command){
                                    close(fds[command_order-1][0]);
                                }
                                //pipeline process
                                first_command = 0;
                                if(cmdlist_ptr!=NULL&&cmdlist_ptr->rest==NULL){
                                    last_command = 1;
                                }
                                if(cmdlist_ptr==NULL) {
                                    int command_status;
                                    while ((waitpid(-1,&command_status,0))!=-1){//wait(&command_status)!=-1){
                                        if(WIFEXITED(command_status)==0){//if command aborted
                                            //printf("command abort status is %d\n", WEXITSTATUS(command_status));
                                            abort();//abort pipeline
                                        }
                                    }
                                    exit(WEXITSTATUS(command_status));
                                    break;
                                }
                                current_cmd = cmdlist_ptr->first;
                                cmdlist_ptr = cmdlist_ptr->rest;
                            }
                        }
                        int pipeline_status;
                        wait(&pipeline_status);//pipeline sequential
                        if(WIFEXITED(pipeline_status)==0){//if last pipeline aborted
                            abort();//abort runner
                        }
                        if(pipelist_ptr==NULL){ break;}
                        current_pipeline = pipelist_ptr->first;
                        pipelist_ptr = pipelist_ptr->rest;
                    }
                    int pipe_status;
                    while ((waitpid(-1,&pipe_status,0))!=-1) { ;}//wait for all pipeline, command process
                    exit(WEXITSTATUS(pipe_status));//runner completes
                    //copy end
            }
            sf_job_start(newfork, getpid());
            sf_job_status_change(newfork, WAITING, RUNNING);
            jobs_table[newfork].job_pgid = pid;
        }
    }
    if(ccount==0)
        done = 1;
    sigprocmask(SIG_SETMASK, &prev_one, NULL);
    errno = olderrno;
}

void jobs_run(){//print start or error stmt
    int index;
    while((index = smallest_runnable())!=-1&&running<MAX_RUNNERS){
        running++;
        jobs_table[index].job_status = RUNNING;
    }
    signal(SIGCHLD, sigchld_handler);
    //signal(SIGABRT, abort_handler);
        for(int i=0;i<MAX_JOBS;i++){
            if(jobs_table[i].job_status==RUNNING&&running!=0){
                ccount++;
                //running--;
                int pid;
                if((pid = fork())==0){//child, runner process
                    //sleep(1);
                    TASK* task = jobs_table[i].job_task;
                    signal(SIGCHLD, pipeline_handler);
                    PIPELINE_LIST* pipelist_ptr = task->pipelines->rest;
                    PIPELINE* current_pipeline = task->pipelines->first;
                    //pcount = 0;
                    while(1){
                        int pgid = getpid();//runner process
                        setpgid(getpid(), pgid);
                        //pcount++;
                        if(fork()==0){//pipeline process
                            //printf("pipeline pid: %d pgid: %d\n", getpid(), pgid);
                            setpgid(getpid(), pgid);
                            //start command process
                            COMMAND_LIST* cmdlist_ptr = current_pipeline->commands->rest;
                            COMMAND* current_cmd= current_pipeline->commands->first;
                            int command_count = 0;
                            if(current_cmd!=NULL)
                                command_count++;
                            COMMAND_LIST* count_ptr = cmdlist_ptr;
                            while(count_ptr!=NULL){
                                command_count++;
                                count_ptr = count_ptr->rest;
                            }
                            pcount = 0;//number of command process, in pipe process
                            int first_command = 1;
                            int last_command = 0;
                            int command_order = -1;
                            int fds[command_count][2];
                            for(int i=0;i<command_count;i++){
                                pipe(fds[i]);
                            }
                            while(1){
                                pcount++;
                                signal(SIGCHLD, cmd_handler);
                                command_order++;
                                if(fork()==0){//fork command process
                                    //printf("command: %s pid: %d pgid: %d\n",current_cmd->words->first, getpid(), pgid);
                                    if(command_count==1){
                                        if(current_pipeline->input_path!=NULL){
                                            int in = open(current_pipeline->input_path, O_RDONLY);
                                            if(in==-1)
                                                abort();
                                            dup2(in,0);
                                            close(in);
                                        }
                                        if(current_pipeline->output_path!=NULL){
                                            int out = open(current_pipeline->output_path, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
                                            dup2(out,1);
                                            close(out);
                                        }
                                    }
                                    else{//multiple commands
                                        if(first_command){
                                            //printf("first command\n");
                                            close(fds[command_order][0]);
                                            dup2(fds[command_order][1], 1);
                                            close(fds[command_order][1]);
                                        }
                                        else if(last_command){
                                            //printf("last_command\n" );
                                            int in = open(current_pipeline->input_path, O_RDONLY);
                                            int out = open(current_pipeline->output_path, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
                                            if(in==-1)
                                                abort();
                                            close(fds[command_order][1]);
                                            dup2(in, STDIN_FILENO);
                                            dup2(out, STDOUT_FILENO);
                                            if(in!=STDIN_FILENO) close(in);
                                            if(out!=STDOUT_FILENO) close(out);
                                            dup2(fds[command_order-1][0], 0);
                                            close(fds[command_order-1][0]);
                                        }
                                        else{
                                            //printf("middle command\n");
                                            close(fds[command_order][0]);
                                            dup2(fds[command_order][1], 1);
                                            close(fds[command_order][1]);
                                            dup2(fds[command_order-1][0], 0);
                                            close(fds[command_order-1][0]);
                                        }
                                    }
                                    setpgid(getpid(), pgid);
                                    int command_arg_length = 0;
                                    WORD_LIST* rest_ptr = current_cmd->words->rest;
                                    while(rest_ptr!=NULL){
                                        command_arg_length++;
                                        rest_ptr = rest_ptr->rest;
                                    }
                                    char* args[command_arg_length+2];
                                    args[0] = current_cmd->words->first;
                                    rest_ptr = current_cmd->words->rest;
                                    for(int i=0;i<command_arg_length;i++){
                                        args[i+1] = rest_ptr->first;
                                        rest_ptr = rest_ptr->rest;
                                    }
                                    args[command_arg_length+1] = NULL;
                                    if(execvp(current_cmd->words->first, args)==-1){
                                        perror("execvp failed");
                                        abort();//aborted command
                                    }
                                    exit(0);//end command
                                }
                                close(fds[command_order][1]);
                                if(!first_command){
                                    close(fds[command_order-1][0]);
                                }
                                //pipeline process
                                first_command = 0;
                                if(cmdlist_ptr!=NULL&&cmdlist_ptr->rest==NULL){
                                    last_command = 1;
                                }
                                if(cmdlist_ptr==NULL) {
                                    int command_status;
                                    while ((waitpid(-1,&command_status,0))!=-1){//wait(&command_status)!=-1){
                                        if(WIFEXITED(command_status)==0){//if command aborted
                                            //printf("command abort status is %d\n", WEXITSTATUS(command_status));
                                            abort();//abort pipeline
                                        }
                                    }
                                    //printf("command exit status is %d\n", WEXITSTATUS(command_status));
                                    exit(WEXITSTATUS(command_status));
                                    break;
                                }
                                current_cmd = cmdlist_ptr->first;
                                cmdlist_ptr = cmdlist_ptr->rest;
                            }
                        }
                        int pipeline_status;
                        wait(&pipeline_status);//pipeline sequential
                        if(WIFEXITED(pipeline_status)==0){//if last pipeline aborted
                            abort();//abort runner
                        }
                        if(pipelist_ptr==NULL){ break;}
                        current_pipeline = pipelist_ptr->first;
                        pipelist_ptr = pipelist_ptr->rest;
                    }
                    int pipe_status;
                    while ((waitpid(-1,&pipe_status,0))!=-1) { ;}//wait for all pipeline, command process
                    //printf("pipeline exit status is %d\n", WEXITSTATUS(pipe_status));
                    exit(WEXITSTATUS(pipe_status));//runner completes
                }
                sf_job_start(i, pid);
                sf_job_status_change(i, WAITING, RUNNING);
                jobs_table[i].job_pgid = pid;
            }
        }
        //while(!done){;}
        done = 0;
        //printf("finished handler\n");
}

int smallest_runnable(){
    for(int i=0;i<MAX_JOBS;i++){
        if(job_get_status(i)==WAITING)
            return i;
    }
    return -1;
}

int exist_running_job(){
    for(int i=0;i<MAX_JOBS;i++){
        if(jobs_table[i].job_status==RUNNING)
            return 1;
    }
    return 0;
}

void job_set_status(int jobid, JOB_STATUS status){
    jobs_table[jobid].job_status = status;
}

int hook_func(){
    signal(SIGCHLD, sigchld_handler);
    //printf("hook function\n");
    return 0;
}