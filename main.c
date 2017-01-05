/******************************************************************************
 *
 *  File Name........: main.c
 *
 *  Description......: Simple driver program for ush's parser
 *
 *  Author...........: Vincent W. Freeh
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "parse.h"
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/types.h>


static void prCmd(Cmd c);
void ush_setenv(Cmd c);
void ush_unsetenv(Cmd c);
int isDirectory(const char *path);
void ush_cd(Cmd c);
void ush_echo(Cmd c);
void ush_pwd(Cmd c);
void ush_where(Cmd c);
void ush_nice(Cmd c);
void redirect_io(Cmd c);
void ush_exec_extern(Cmd c);
void ush_runCmd(Cmd c);
static void prPipe(Pipe p);
void sigint_handler(int signo);
void initialize_pipes();
int check_extern(Cmd c, int offset);


extern char **environ;
int backup_in, backup_out, backup_err, pipe0[2], pipe1[2], lock, inchild, rc_file_processing, ended;
char *host;

static void prCmd(Cmd c)
{
  int i;

  if ( c ) {
    printf("%s%s ", c->exec == Tamp ? "BG " : "", c->args[0]);
    if ( c->in == Tin )
      printf("<(%s) ", c->infile);
    if ( c->out != Tnil )
      switch ( c->out ) {
      case Tout:
	printf(">(%s) ", c->outfile);
	break;
      case Tapp:
	printf(">>(%s) ", c->outfile);
	break;
      case ToutErr:
	printf(">&(%s) ", c->outfile);
	break;
      case TappErr:
	printf(">>&(%s) ", c->outfile);
	break;
      case Tpipe:
	printf("| ");
	break;
      case TpipeErr:
	printf("|& ");
	break;
      default:
	fprintf(stderr, "Shouldn't get here\n");
	exit(-1);
      }

    if ( c->nargs > 1 ) {
      printf("[");
      for ( i = 1; c->args[i] != NULL; i++ )
	printf("%d:%s,", i, c->args[i]);
      printf("\b]");
    }
    putchar('\n');
    // this driver understands one command
    if ( !strcmp(c->args[0], "end") )
      exit(0);
  }
}


static void prPipe(Pipe p)
{
	Cmd c;
	if (p == NULL)
	return;
	lock = 0;
	initialize_pipes();
	
//	printf("outside prpipe loop\n");
//	printf("Begin pipe%s\n", p->type == Pout ? "" : " Error");
	for (c = p->head; c != NULL; c = c->next) 
	{
		if(strcmp(c->args[0], "end") == 0)
		{
			if(rc_file_processing == 1)
			{
				return;
			}
			else
			{
				exit(0);
			}
		}
//		printf("Cmd #%d: ", ++i);
		ush_runCmd(c);
//		prCmd(c);

	}
//	printf("End pipe\n");
	prPipe(p->next);
}


void sigint_handler(int signo)
{
	printf("\r\n"); // goto next line
	if(inchild == 0)
	printf("%s%% ", host);
	fflush(STDIN_FILENO);
	return;
}


void sigterm_handler(int signo)
{
	killpg(getpgrp(),SIGTERM);
	exit(0);
}

void initialize_pipes()
{
	if(pipe(pipe0) != 0)
	{
		perror("");
	}
	if(pipe(pipe1) != 0)
	{
		perror("");
	}
}


int main(int argc, char *argv[])
{
	Pipe p;
//	char *host = "DarkShell";
	host = malloc(4096*sizeof(char));
	char *home_path, *rc_path;
	int rc_file;
	inchild = 0;
	home_path = malloc(4096*sizeof(char));
	rc_path = malloc(4096*sizeof(char));

	backup_in = dup(STDIN_FILENO);
	backup_out = dup(STDOUT_FILENO);
	backup_err = dup(STDERR_FILENO);
	
	home_path = getenv("HOME");
//	printf("%s\n", home_path);
	strcpy(rc_path, home_path);
	strcat(rc_path, "/.ushrc");
//	printf("%s\n", rc_path);
	
	rc_file = open(rc_path, O_RDONLY);
	
	if(rc_file < 0)
	{
		printf("~/.ushrc not found\r\n");
	}
	else
	{
		rc_file_processing = 1;
		dup2(rc_file, 0);
		close(rc_file);
		lock = 0;
		initialize_pipes();
		fflush(stdout);
		p = parse();
		inchild = 1;
		prPipe(p);
		freePipe(p);
		inchild = 0;

		dup2(backup_in, 0);
		close(backup_in);
		dup2(backup_out, 1);
		close(backup_out);
		dup2(backup_err, 2);
		close(backup_err);
		inchild = 0;

		rc_file_processing = 0;
	}
	
	backup_in = dup(STDIN_FILENO);
	backup_out = dup(STDOUT_FILENO);
	backup_err = dup(STDERR_FILENO);

	if(gethostname(host, 4096) !=0)
	{
		perror("error in gethostname");
		exit(1);
	}
	signal(SIGINT, sigint_handler);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTERM, sigterm_handler);
	if(isatty(fileno(stdout)))
	{	
		printf("%s%% ", host);
		fflush(stdout);
	}
	while(1)
	{		
		dup2(backup_in, 0);
		close(backup_in);
		dup2(backup_out, 1);
		close(backup_out);
		dup2(backup_err, 2);
		close(backup_err);
	
		lock = 0;
		initialize_pipes();

		p = parse();
		
		if(p==NULL)
		{
//			printf("%s%% ", host);
//			fflush(stdout);
		}

		inchild = 1;
		prPipe(p);
		freePipe(p);

		backup_in = dup(STDIN_FILENO);
		backup_out = dup(STDOUT_FILENO);
		backup_err = dup(STDERR_FILENO);

		inchild = 0;
		if(isatty(fileno(stdout)))
		{	
			printf("%s%% ", host);
			fflush(stdout);
		}	
		ended = 0;
	}
}


void redirect_io(Cmd c)
{
	int in, out, err;
	switch(c->in)
	{
	case Tin:
		in = open(c->infile, O_RDONLY);
		if(in == -1)
		{
			fprintf(stderr, "%s: ", c->infile);
			perror("");
			exit(1);
		}
		dup2(in, STDIN_FILENO);
		close(in);
		break;
	case Tpipe:
		if(lock == 0)
		{
			dup2(pipe0[0], STDIN_FILENO); //copy reading end of pipe to stdin
			close(pipe0[0]); //close reading end of pipe since it is unused
			close(pipe0[1]); //close writing end of pipe since it is unused
		}
		else
		{
			dup2(pipe1[0], STDIN_FILENO); //copy reading end of pipe to stdin
			close(pipe1[0]); //close reading end of pipe since it is unused
			close(pipe1[1]); //close writing end of pipe since it is unused
		}
	case TpipeErr:
			if(lock == 0)
		{
			dup2(pipe0[0], STDIN_FILENO); //copy reading end of pipe to stdin
			dup2(pipe0[0], STDERR_FILENO); //copy reading end of pipe to stderr
			close(pipe0[0]); //close reading end of pipe since it is unused
			close(pipe0[1]); //close writing end of pipe since it is unused
		}
		else
		{
			dup2(pipe1[0], STDIN_FILENO); //copy reading end of pipe to stdin
			dup2(pipe0[0], STDERR_FILENO); //copy reading end of pipe to stderr
			close(pipe1[0]); //close reading end of pipe since it is unused
			close(pipe1[1]); //close writing end of pipe since it is unused
		}
	}

	if(c->out != Tnil)
	{
		switch(c->out)
		{
		case Tout:
			out = open(c->outfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
			if(out == -1)
			{
				fprintf(stderr, "%s: ", c->outfile);
				perror("");
				exit(1);
			}
			dup2(out,STDOUT_FILENO);
			close(out);
			break;
		case Tapp:
			out = open(c->outfile, O_CREAT|O_APPEND|O_WRONLY, 0644);
			if(out == -1)
			{
				fprintf(stderr, "%s: ", c->outfile);
				perror("");
				exit(1);
			}
			dup2(out,STDOUT_FILENO);
			close(out);
			break;
		case ToutErr:
//			printf("ToutErr\n");
			out = open(c->outfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
			if(out == -1)
			{
				fprintf(stderr, "%s: ", c->outfile);
				perror("");
				exit(1);
			}
			dup2(out,STDOUT_FILENO);
			dup2(STDOUT_FILENO,STDERR_FILENO);
			close(out);
			break;
		case TappErr:
//			printf("TappErr\n");
			out = open(c->outfile, O_CREAT|O_APPEND|O_WRONLY, 0644);
			if(out == -1)
			{
				fprintf(stderr, "%s: ", c->outfile);
				perror("");
				exit(1);
			}
			dup2(out,STDOUT_FILENO);
			dup2(STDOUT_FILENO,STDERR_FILENO);
			close(out);
			break;
		case Tpipe:
//			printf("Tpipe\n");
			if(lock == 1)
			{
				dup2(pipe0[1],STDOUT_FILENO);
				close(pipe0[0]);
				close(pipe0[1]);
			}
			else
			{
				dup2(pipe1[1],STDOUT_FILENO);
				close(pipe1[0]);
				close(pipe1[1]);
			}
		case TpipeErr:
//			printf("TpipeErr\n");
			if(lock == 1)
			{
				dup2(pipe0[1],STDOUT_FILENO);
				dup2(pipe0[1],STDERR_FILENO);
				close(pipe0[0]);
//				close(pipe0[1]);
			}
			else
			{
				dup2(pipe1[1],STDOUT_FILENO);
				dup2(pipe1[1],STDERR_FILENO);
				close(pipe1[0]);
//				close(pipe1[1]);
			}
		}
	}
}


void correct_redirect_io(Cmd c)
{
	ended = 1;
	if( ((c->in==Tnil) || (c->in == Tin))  && (c->out==Tpipe || c->out==TpipeErr) ) // if this is the first command
	{

		close(pipe1[1]);
		lock=1;
	}
	else if(c->in==Tpipe || c->in==TpipeErr) 
	{
		if(lock==1)
		{
			close(pipe0[1]);
			lock=0;
		}
		else if(lock==0)
		{
			close(pipe1[1]);
			lock=1;
		}
//		lock = 1 - lock;
	}

	if( (c->in==Tpipe||c->in==TpipeErr) && (c->out==Tpipe|| c->out==TpipeErr) ) // if this is a command in between
	{
		if(lock==1)
		{
			if(pipe(pipe0) != 0)
			{
				perror("");
			}
		}
		else if(lock==0)
		{
			if(pipe(pipe1) != 0)
			{
				perror("");
			}
		}
	}
}


int check_extern(Cmd c, int offset)
{
	int i;
	for(i = 0; i<strlen(c->args[offset]); i++)
	{
		if(c->args[offset][i] == '/') // if a / is present, consider it a path
		{
			if(access(c->args[offset], F_OK) != 0) // return if path does not exist
			{
				fprintf(stderr, "command not found\n");
//				printf("command not found\n");
				return 0;
			}
			else if(isDirectory(c->args[offset]) == 1) // return if the path is a directory
			{
				fprintf(stderr, "permission denied\n");
//				printf("permission denied\n");
				return 0;
			}
			else if(access(c->args[offset], X_OK) != 0) // return if it is a file without execute permissions
			{
				fprintf(stderr, "permission denied\n");
//				printf("permission denied\n");
				return 0;
			}
		}
	}
	return 1;
}


void ush_exec_extern(Cmd c)
{
	int i;
	pid_t pid;
	pid = fork();
	if(pid<0) // if fork failed
	{
		perror("fork error");
		exit(1);
	}
	else if(pid == 0) // child
	{
		redirect_io(c);
		int valid_comm;
		valid_comm = check_extern(c, 0);
//		printf("c->args[0] = %s\n", c->args[0]);
		if(valid_comm == 1)
		{
			if(execvp(c->args[0], c->args) == -1)
			{
				perror("");
				exit(1);
			}
		}
		else
		{			
			exit(0);
		}
	}
	else // parent
	{
		wait();
		correct_redirect_io(c);
	}
//	}
}


void ush_nice(Cmd c)
{
	int which = PRIO_PROCESS;
	id_t who;
	int prio = 4;
	int offset = 0;
	if(c->args[1] != NULL)// if number is specified
	{
		if(c->args[1][0] == '-') // if the first alphabet is '-' check if c->args[1][1] is a number
		{
			offset = 1;
		}
		if(isdigit(c->args[1][offset]) != 0) // proceed only if c->args[1] is a number
		{
			prio = atoi(c->args[1]);
//			printf("prio = %d\n", prio);
		}
		else
		{///////////////////////////////////////////syntax error/////////////////////////////////////////
			fprintf(stderr, "Usage: nice [[+/-]number] [command]\n");
			return;
		}
	}
	if(c->args[2] == NULL) // if command is not specified set own priority to number or 4
	{
//		printf("%d\n", prio);
		who = getpid();
		if(setpriority(which,who,prio) != 0)
		{
			perror("setpriority error");
			exit(1);
		}
//		else
//		printf("priority is: %d\n", getpriority(which,who));
	}
	else // if command is specified set its priority to number and proceed
	{
		pid_t pid;
		pid = fork();
		if(pid<0) // if fork failed
		{
			perror("fork error");
			exit(1);
		}
		else if(pid == 0) // child
		{
			int valid_comm;
			redirect_io(c);

			if(setpriority(which,getpid(),prio) != 0)
			{
				perror("setpriority error");
				exit(1);
			}

			valid_comm = check_extern(c, 2);
			if(valid_comm == 1)
			{
				if(execvp(c->args[2], c->args +2) == -1)
				{
					perror("");
					exit(1);
				}
			}

			else
			{			
				exit(1);
			}

/*			if(execvp(c->args[2], c->args+2) == -1)
			{
				perror("");
				exit(1);
			}
*/		}
		else // parent
		{
			wait();
			correct_redirect_io(c);
		}
	}
}


void ush_setenv(Cmd c)
{
	char **env_curr = environ;
	int env_present = 0;
	int l, new_l;
	char *newval, *newvar;

	if(c->args[1] == NULL) // if only setenv is called
	{
		pid_t pid;
		pid = fork();
		if(pid<0) // if fork failed
		{
			perror("fork error");
			exit(0);
		}
		else if(pid == 0) // child
		{
			redirect_io(c);
			while(*env_curr!=NULL)
			{
				printf("%s\n", *env_curr);
				env_curr++;
			}
			exit(0);
		}
		else
		{
			wait(0);
			correct_redirect_io(c);
		}
	}
	else
	{
		l = strlen(c->args[1]);
		if(c->args[2] == NULL) // if setenv VAR is called, set the value of VAR to NULL
		{
			newval = (char*) malloc (0*sizeof(char));
			strcpy(newval, "");
//			printf("%s\n", newval);
		}
		else
		{
			newval = (char*) malloc (strlen(c->args[2])*sizeof(char));
			strcpy(newval, c->args[2]);
//			printf("%s\n", newval);
		}
		new_l = l+1+strlen(newval);
//		printf("%d\n", new_l);
		newvar = (char*) malloc (new_l*sizeof(char));
		strcat(newvar, c->args[1]);
		strcat(newvar , "=");
		strcat(newvar, newval);
		putenv(newvar);
//		printf("%s\n", newvar);
/*		while(*env_curr!=NULL)
		{
			if(strncmp(*env_curr, c->args[1], l) == 0)
			{
				env_present = 1;
//				printf("found env var: %s\n",c->args[1]);
				break;
			}
			env_curr++;
		}
*/	}
//	printf("exiting ush_setenv\n");
}


void ush_unsetenv(Cmd c)
{
	unsetenv(c->args[1]);
}


int isDirectory(const char *path) 
{
	struct stat stat_path;
	int isdir;
	if (stat(path, &stat_path) != 0)
	{
		isdir = 0;
	}
	else
	{
		isdir = S_ISDIR(stat_path.st_mode);
	}
	return isdir;
}


void ush_cd(Cmd c)
{
	char *path;
	path = (char*) malloc (4096*sizeof(char));
	if(c->args[1] == NULL)
	{
		strcpy(path,"/home");
	}
	else
	{
		strcpy(path,c->args[1]);
	}
	
	if(isDirectory(path) == 1) // if it is a directory
	{
		if(access(path, X_OK) == 0)// if execute permissions are there for the directory
		{
			chdir(path);
		}
		else // if execute permissions are not there for the directory
		{	
			pid_t pid;
			pid = fork();
			if(pid<0) // if fork failed
			{
				perror("fork error");
				exit(1);
			}
			else if(pid == 0) // child
			{
				redirect_io(c);	
				fprintf(stderr, "%s: access denied\n", path);
				exit(0);
			}
			else // parent
			{
				wait();
				correct_redirect_io(c);
			}
		}
	}
	else // if it is not a directory
	{
		pid_t pid;
		pid = fork();
		if(pid<0) // if fork failed
		{
			perror("fork error");
			exit(1);
		}
		else if(pid == 0) // child
		{
			redirect_io(c);	
			fprintf(stderr, "%s: no such directory\n", path);
			exit(0);
		}
		else // parent
		{
			wait();
			correct_redirect_io(c);
		}
	}
}


void ush_echo(Cmd c)
{
	pid_t pid;
	pid = fork();
	if(pid<0) // if fork failed
	{
		perror("fork error");
		exit(1);
	}
	else if(pid == 0) // child
	{
		redirect_io(c);
		int i;
		for(i = 1; c->args[i]!= NULL; i++)
		{
			if(c->args[i+1] != NULL)
			printf("%s ", c->args[i]);
			else
			printf("%s", c->args[i]);
		}
		printf("\n");
		exit(0);
	}
	else // parent
	{
		wait();
		correct_redirect_io(c);
	}
}


void ush_pwd(Cmd c)
{
	pid_t pid;
	pid = fork();
	if(pid<0) // if fork failed
	{
		perror("fork error");
		exit(1);
	}
	else if(pid == 0) // child
	{	
		redirect_io(c);
		char cwd[4096]; //since maximum path length in linux can be 4096 characters only
		getcwd(cwd, 4096);
		if(cwd == NULL)
		{
			printf("getcwd failed\n");
		}
		else
		{
			printf("%s\n", cwd);
		}
		exit(0);
	}
	else
	{
		wait();
		correct_redirect_io(c);
	}
}


void ush_where(Cmd c)
{
	pid_t pid;
	pid = fork();
	if(pid<0) // if fork failed
	{
		perror("fork error");
		exit(1);
	}
	else if(pid == 0) // child
	{
		redirect_io(c);
		int found;
		found = 0;
		if(c->args[1] != NULL)
		{
			if((!strcmp(c->args[1], "cd")) || (!strcmp(c->args[1], "echo")) || (!strcmp(c->args[1], "logout")) || 
				(!strcmp(c->args[1], "nice")) || (!strcmp(c->args[1], "pwd")) || (!strcmp(c->args[1], "setenv")) || 
				(!strcmp(c->args[1], "unsetenv")) || (!strcmp(c->args[1], "where")))
			{
				printf("%s: Builtin\n", c->args[1]);
				found = 1;
			}
			int i, j, pos, l;
			char *env, *path, *fullpath, *token, **tokens;
			env = getenv("PATH");
			l = strlen(env);
//			printf("%s\n", env);
			path = malloc ((l+1)*sizeof(char));
			strcpy(path, env);
			path[l] = '\0';
//			printf("%s\n", env);
//			printf("%s\n", path);
			j = 0;
			pos = 0;
			for(i = 0; i<l; i++)
			{
				if(env[i] == ':')
				{
					j++;
				}
			}
			tokens = malloc((j+1) * sizeof(char*));
			token = strtok(path, ":");
			while(token != NULL)
			{
				tokens[pos] = token;
//				printf("%s\n", tokens[pos]);
				token = strtok(NULL, ":");
				pos++;
			}
//			printf("%d\n", access("/bin/ls", X_OK));
			for(i = 0; i<pos; i++)
			{
				fullpath = malloc((strlen(tokens[i])+ strlen(c->args[1])+1)*sizeof(char*));
				strcpy(fullpath, tokens[i]);
				strcat(fullpath, "/");
				strcat(fullpath, c->args[1]);
//				printf("%s\n", tokens[i]);
//				printf("%s\n", fullpath);
				if(!access(fullpath, R_OK))
				{
					printf("%s: %s\n", c->args[1], fullpath);
					found = 1;
				}
			}
			if(!found)
			{
				printf("Command not found in builtin or PATH\n");
			}
		}
		else
		{//////////////////////////////////////syntax error/////////////////////////////////////////////////////////////
			fprintf(stderr, "Usage: where [command]\n");
		}
		exit(0);
	}
	else
	{
		wait();
		correct_redirect_io(c);
	}
}


void ush_runCmd(Cmd c)
{
	if(!strcmp(c->args[0], "bg")) // extra credit
	{
		printf("bg requested\n");
	}
	else if(!strcmp(c->args[0], "cd")) //done
	{
//		printf("cd requested\n");
		ush_cd(c);
	}
	else if(!strcmp(c->args[0], "fg")) // extra credit
	{
		printf("fg requested\n");
	}
	else if(!strcmp(c->args[0], "echo")) //done, redirect_io remaining
	{
//		printf("echo requested\n");
		ush_echo(c);
	}
	else if(!strcmp(c->args[0], "jobs")) // extra credit
	{
		printf("jobs requested\n");
	}
	else if(!strcmp(c->args[0], "kill")) // extra credit
	{
		printf("kill requested\n");
	}
	else if(!strcmp(c->args[0], "logout")) //done
	{
//		printf("logout requested\n");
		exit(0);
	}
	else if(!strcmp(c->args[0], "nice")) //done, redirect_io remaining
	{
//		printf("nice requested\n");
		ush_nice(c);
	}
	else if(!strcmp(c->args[0], "pwd")) //done
	{
//		printf("pwd requested\n");
		ush_pwd(c);
	}
	else if(!strcmp(c->args[0], "setenv")) //done
	{
//		printf("setenv requested\n");
		ush_setenv(c);
	}
	else if(!strcmp(c->args[0], "unsetenv")) //done
	{
//		printf("unsetenv requested\n");
		ush_unsetenv(c);
	}
	else if(!strcmp(c->args[0], "where")) //done
	{
//		printf("where requested\n");
		ush_where(c);
	}
	else
	{
//		printf("not a builtin command\n");
		ush_exec_extern(c);
	}
}


/*........................ end of main.c ....................................*/
