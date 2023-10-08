/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#define MAXARGS   128

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
void ud_history(); //update history file

char** history;
int history_size;
int history_idx; //현재 shell에서부터 저장할 history idx
int argc; char* argv[MAXARGS];
FILE* fp;
char* cmdline;

int main() 
{
    //char cmdline[MAXLINE]; /* Command line */
	history_size = 1; //현재 history 개수+1
	//history 배열 동적 할당
	history = (char**)malloc(sizeof(char*)*history_size);
	fp = fopen("history.txt", "a+");
	if(fp != NULL){
		while(!feof(fp)){
			char* cmd = (char*)malloc(sizeof(char)*MAXLINE);
			fgets(cmd, MAXLINE, fp); //파일에 저장된 command 읽기
			if(feof(fp)) break;
			history[history_size-1] = cmd; //history 배열에 저장
			//printf("history file list\n");
			//printf("%d %s", history_size, history[history_size-1]);
			history = (char**)realloc(history, sizeof(char*)*(++history_size));
		}
	}
	history_idx = history_size-1;
    while (1) {
	cmdline = (char*)malloc(sizeof(char)*MAXLINE);
	/* Read */
	printf("> ");                   
	fgets(cmdline, MAXLINE, stdin); //read a line
	
	if (feof(stdin))
		exit(0);

	//history
	if(history_size != 1){ //history 정보 없는 경우 실행 x
		char* newcmd = (char*)malloc(sizeof(char)*MAXLINE);
		int j = 0; //어디서부터 복사할지, !!다음 글자
		for(int i=0;i<strlen(cmdline);i++){
			//!!가 나온 부분을 찾는다
			if(cmdline[i] == '!'){
				if(cmdline[i+1] == '!'){
					strncat(newcmd, cmdline+j, i-j); //i앞까지복사
					strncat(newcmd, *(history+(history_size-2)), strlen(history[history_size-2])-1); //!! 대신, \n제외
					j = i+2;
					i++;
				}
				//!#
				else if('0' <= cmdline[i+1] && cmdline[i+1] <= '9'){
					int cnt = 0; //숫자 개수
					int* num = (int*)malloc(sizeof(int)*MAXLINE);
					for(int l=i+1;l<strlen(cmdline);l++){
						//숫자 찾기
						if('0' <= cmdline[l] && cmdline[l] <= '9'){
							//printf("%c\n", cmdline[l]);
							num[cnt++] = cmdline[l] - '0';
						}
						else break;
					}
					int number = 0;
					for(int l=0;l<cnt;l++){
						int sq = 1;
						for(int k=0;k<cnt-l-1;k++){
							sq *= 10;
						}
						number += sq*num[l];
					}
					free(num);
					//printf("number = %d\n", number);
					strncat(newcmd, cmdline+j, i-j); //!앞 부분
					strncat(newcmd, *(history+(number-1)), strlen(history[number-1])-1); //!# 대체
					//strncat(newcmd, cmdline+(i+cnt+1), strlen(cmdline)-(i+cnt+1));
					j = i+cnt+1;
				}
			}
		}
		//!! 마지막 뒷부분 남아있으면 붙이기
		
			if(j < strlen(cmdline)){
				strncat(newcmd, cmdline+j, strlen(cmdline)-j);
			}
		/*else{
			if(h < strlen(cmdline)){
				strncat(newcmd, cmdline+h, strlen(cmdline)-h);
				printf("%s", newcmd);
			}
		}*/
		strcpy(cmdline, newcmd);
		//printf("newcmd : %s-------\n", cmdline);

	}

	if(history_size == 1){ //처음은 그냥 저장
		history[history_size-1] = cmdline;
		//printf("11history[%d] %s", history_size-1, history[history_size-1]);
		history = (char**)realloc(history, sizeof(char*)*(++history_size));
	}
	else{ //2번째부터는 중복 검사 후 저장
		if(strcmp(history[history_size-2], cmdline)){
			history[history_size-1] = cmdline;
			//printf("22history[%d] %s", history_size-1, history[history_size-1]);
			history = (char**)realloc(history, sizeof(char*)*(++history_size));
		}
	}
	/* Evaluate */
	eval(cmdline);
    } 
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    //char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    int child_status;

    strcpy(buf, cmdline); //buf <= cmdline
    bg = parseline(buf, argv); //check input ended in '&'

    if (argv[0] == NULL) //명령어가 없으면
	return;   /* Ignore empty lines */
	

    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
	//builtin command가 아닌 경우 -> fork()
		if((pid = fork()) == 0){
			if (execvpe(argv[0], argv, environ) < 0) {	//ex) /bin/ls ls -al &
            	printf("%s: Command not found.\n", argv[0]);
				exit(0);
        	}
		}
	/* Parent waits for foreground job to terminate */
		if (!bg){ 
	    	//int status;
			if(waitpid(pid, &child_status, 0)<0){
				unix_error("waitfh: waitpid error");
			}
		}
		else//when there is backgrount process!
	    	printf("%d %s", pid, cmdline);
    }
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "quit")){ /* quit command */
		ud_history();
		//fclose(fp);
		//free(history);
		exit(0);  
	}
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
		return 1;
    if (!strcmp(argv[0], "cd")){
		if(chdir(argv[1]) == -1){
			printf("fail\n");
		}
		return 1;
	}
	if (!strcmp(argv[0], "exit")){
		ud_history();
		//fclose(fp);
		//free(history);
		exit(0);
	}
	if (!strcmp(argv[0], "history")){
		for(int i=0;i<history_size-1;i++){
			printf("%d %s", i+1, history[i]);
		}
		return 1;
	}
    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    //int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
	return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
	argv[--argc] = NULL;

    return bg;
}
/* $end parseline */

/*save history.txt*/
void ud_history(){
	for(int i=history_idx;i<history_size-1;i++){
		fputs(history[i], fp);
	}
	fclose(fp);
	free(history);
}
