#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#include "jobber.h"
#include "task.h"
#include "helper.h"


/*
 * "Jobber" job spooler.
 */

int main(int argc, char *argv[])
{
    jobs_init();
    flag = 0;
    running = 0;
    while(1){
        char* user_input = sf_readline("jobber> ");
        if(user_input==NULL)
            exit(EXIT_SUCCESS);
        if(*user_input==0)
            continue;
        char* keyword = strtok(user_input, " ");
        if(strcmp(keyword,"help")==0){
            printf("Available commands:\n"
                "help (0 args) Print this help message\n"
                "quit (0 args) Quit the program\n"
                "enable (0 args) Allow jobs to start\n"
                "disable (0 args) Prevent jobs from starting\n"
                "spool (1 args) Spool a new job\n"
                "pause (1 args) Pause a running job\n"
                "resume (1 args) Resume a paused job\n"
                "cancel (1 args) Cancel an unfinished job\n"
                "expunge (1 args) Expunge a finished job\n"
                "status (1 args) Print the status of a job\n"
                "jobs (0 args) Print the status of all jobs\n");
        }
        else if(strcmp(keyword,"quit")==0){
            jobs_fini();
            exit(EXIT_SUCCESS);
        }
        else if(strcmp(keyword,"status")==0){//checked
            char *arg;
            arg = strtok(NULL," ");
            char* real_arg = arg;
            int count;
            arg = strtok(NULL," ");
            if(arg==NULL&&real_arg==NULL) count = 0;
            else{
                count = 1;
                while(arg!=NULL){
                    arg = strtok(NULL," ");
                    count++;
                }
            }
            if(count!=1){
                printf("Wrong number of args (given: %d, required: 1) for command 'status'\n", count);
                continue;
            }
            if(strlen(real_arg)!=1||!isdigit(*real_arg)){ continue;}
            int i = ((int)*real_arg)-48;
            if(job_get_taskspec(i)==NULL) continue;
            printf("job %d [%s]: %s\n", i, job_status_names[job_get_status(i)], job_get_taskspec(i));
        }
        else if(strcmp(keyword,"jobs")==0){
            if(jobs_get_enabled()==0)
                printf("Starting jobs is disabled\n");
            else
                printf("Starting jobs is enabled\n");
            for(int i=0;i<MAX_JOBS;i++){
                if(job_get_taskspec(i)!=NULL)
                    printf("job %d [%s]: %s\n", i, job_status_names[job_get_status(i)], job_get_taskspec(i));
                  //printf("job %d [%s]: %s\n", i, job_status_names[job_get_status(i)], job_get_taskspec(i));
            }
        }
        else if(strcmp(keyword,"enable")==0){
            jobs_set_enabled(1);
            jobs_run();
        }
        else if(strcmp(keyword,"disable")==0){
            jobs_set_enabled(0);
        }
        else if(strcmp(keyword,"spool")==0){
            char *arg;
            if(*(user_input+6)!='\'')
                arg = strtok(NULL," ");
            else{
                arg = strtok(NULL,"\'");
                if(arg==0){
                    printf("Error: spool\n");
                    continue;
                }
            }
            char* real_arg = arg;
            int count;
            arg = strtok(NULL," ");
            if(arg==NULL&&real_arg==NULL)
                count = 0;
            else{
                count = 1;
                while(arg!=NULL){
                    arg = strtok(NULL," ");
                    count++;
                }
            }
            if(count!=1){
                printf("Wrong number of args (given: %d, required: 1) for command 'spool'\n", count);
                continue;
            }
            int created;
            if((created = job_create(real_arg))==-1){
                printf("Error: spool\n");
                continue;
            }
            //printf("TASK: %s\n", job_get_taskspec(created));
            sf_job_create(created);
            job_set_status(created, WAITING);
            sf_job_status_change(created,NEW,WAITING);
            if(jobs_get_enabled()==1)
                jobs_run();
            continue;
        }
        else if(strcmp(keyword,"pause")==0){
            char *arg;
            arg = strtok(NULL," ");
            char* real_arg = arg;
            int count;
            arg = strtok(NULL," ");
            if(arg==NULL&&real_arg==NULL) count = 0;
            else{
                count = 1;
                while(arg!=NULL){
                    arg = strtok(NULL," ");
                    count++;
                }
            }
            if(count!=1){
                printf("Wrong number of args (given: %d, required: 1) for command '%s'\n", count, keyword);
                continue;
            }
            if(strlen(real_arg)!=1||!isdigit(*real_arg)){ printf("Error: pause\n"); continue;}
            int i = ((int)*real_arg)-48;
            job_pause(i);
        }
        else if(strncmp(keyword,"resume",6)==0){
            char *arg;
            arg = strtok(NULL," ");
            char* real_arg = arg;
            int count;
            arg = strtok(NULL," ");
            if(arg==NULL&&real_arg==NULL) count = 0;
            else{
                count = 1;
                while(arg!=NULL){
                    arg = strtok(NULL," ");
                    count++;
                }
            }
            if(count!=1){
                printf("Wrong number of args (given: %d, required: 1) for command '%s'\n", count, keyword);
                continue;
            }
            if(strlen(real_arg)!=1||!isdigit(*real_arg)){ printf("Error: resume\n"); continue;}
            int i = ((int)*real_arg)-48;
            job_resume(i);
        }
        else if(strcmp(keyword,"cancel")==0){
            char *arg;
            arg = strtok(NULL," ");
            char* real_arg = arg;
            int count;
            arg = strtok(NULL," ");
            if(arg==NULL&&real_arg==NULL) count = 0;
            else{
                count = 1;
                while(arg!=NULL){
                    arg = strtok(NULL," ");
                    count++;
                }
            }
            if(count!=1){
                printf("Wrong number of args (given: %d, required: 1) for command '%s'\n", count, keyword);
                continue;
            }
            if(strlen(real_arg)!=1||!isdigit(*real_arg)){ printf("Error: cancel\n"); continue;}
            int i = ((int)*real_arg)-48;
            job_cancel(i);
        }
        else if(strcmp(keyword,"expunge")==0){
            char *arg;
            arg = strtok(NULL," ");
            char* real_arg = arg;
            int count;
            arg = strtok(NULL," ");
            if(arg==NULL&&real_arg==NULL) count = 0;
            else{
                count = 1;
                while(arg!=NULL){
                    arg = strtok(NULL," ");
                    count++;
                }
            }
            if(count!=1){
                printf("Wrong number of args (given: %d, required: 1) for command '%s'\n", count, keyword);
                continue;
            }
            if(strlen(real_arg)!=1||!isdigit(*real_arg)){ printf("Error: expunge\n"); continue;}
            int i = ((int)*real_arg)-48;
            job_expunge(i);
        }
        else{
            printf("Unrecognized command: %s\n", keyword);
        }
    }
    exit(EXIT_FAILURE);
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
