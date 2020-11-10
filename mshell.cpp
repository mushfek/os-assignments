/**
 * mShell: A simple Linux Shell
 * 
 * @author Mushfekur Rahman
 * @since 1.0
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

// flag for enabling / disabling status display
int __SHOW_DETAILS__ = 0;
int __CURR_BCKGRND__ = 0;

struct Node {
	rusage ru;
	timeval st, en;
	char name[128];
	int pid, done, sl;
	Node *next;
} *head, *tail;

// functions used for our shell 'mShell' 
void add(Node);
void del(int);
void find(int, Node *);
void changeDirectory(char *);
void printWorkingDirectory(void);
void getCurrentDirectory(char *);
void signalHandler(int);
void showCompletedJobs(int);
void showRunningJobs(void);
void printMessage(timeval *, timeval *, rusage *);

int main(int argc, char **argv) {
	char argument[128], temp[128];
	char *p, sargs[32][128], *pargs[32];
	int i, child_status, background;
	pid_t pid, wait_pid;
	Node b;
	timeval st, en;
	rusage rs;

	signal(SIGCHLD, signalHandler);
	signal(SIGINT, SIG_IGN);

	if(argc > 1) {
		for(i = 1; i < argc; i++) pargs[i-1] = argv[i]; pargs[i-1] = NULL;
		
		pid = fork();

		if(pid == -1) {
			printf("mShell: system call fork() failed...\n");
			exit(1);
		}
		if(!pid) {
			execvp(pargs[0], pargs);
			printf("mShell: %s: command not found...\n", pargs[0]);
			exit(2);
		}
		else {
			gettimeofday(&st, NULL);
			if((wait_pid = waitpid(pid, &child_status, 0)) == -1) {
				printf("mShell: system call waitpid() failed...\n");
				exit(3);
			}
			getrusage(RUSAGE_CHILDREN, &rs);
			gettimeofday(&en, NULL);
			
			if(__SHOW_DETAILS__) {
				printf("PID %d\t[%s] completed.\n", wait_pid, pargs[0]);
				printMessage(&st, &en, &rs);
			}
		}
	}
	else {
		while(1) {
			getCurrentDirectory(temp);
			printf("mShell [%s]$ ", temp);

			fgets(argument, 128, stdin);
			p = strtok(argument, " \n");

			showCompletedJobs(0);

			if(!p) continue;

			for(i = 0; p; i++, p = strtok(0, " \n")) {
				strcpy(sargs[i], p);
				pargs[i] = sargs[i];
			}
			if(i && !strcmp(sargs[i-1], "&")) { background = 1; i--; }
			else background = 0;
			pargs[i] = NULL; sargs[i][0] = 0;

			if(!strcmp(sargs[0], "exit")) {
				showCompletedJobs(1);
				printf("\nmShell: exiting shell...\n");
				return atoi(sargs[1]);
			}
			if(!strcmp(sargs[0], "cd")) {
				changeDirectory(sargs[1]);
				continue;
			}
			if(!strcmp(sargs[0], "pwd")) {
				printWorkingDirectory();
				continue;
			}
			if(!strcmp(sargs[0], "jobs")) {
				showCompletedJobs(1);
				continue;
			}
			if(!strcmp(sargs[0], "@stats")) {
				if(!strcmp(sargs[1], "on")) __SHOW_DETAILS__ = 1;
				else if(!strcmp(sargs[1], "off")) __SHOW_DETAILS__ = 0;
				else printf("Status display: %s\nOptions: [on/off]\n", (__SHOW_DETAILS__? "ON" : "OFF"));
				continue;
			}

			pid = fork();

			if(pid == -1) {
				printf("mShell: system call fork() failed...\n");
				exit(1);
			}
			if(!pid) {
				execvp(pargs[0], pargs);
				printf("mShell: %s: command not found...\n", pargs[0]);
				exit(2);
			}
			else {
				if(background) {
					memset(&b, 0, sizeof(Node));
					b.pid = pid; strcpy(b.name, pargs[0]);
					gettimeofday(&b.st, NULL); b.done = 1;
					b.sl = 1 + __CURR_BCKGRND__;
					add(b);
					printf("[%d] %d\t[%s]\n", b.sl, pid, pargs[0]);
				}
				else {
					gettimeofday(&st, NULL);
					if((wait_pid = waitpid(pid, &child_status, 0)) == -1) {
						printf("mShell: system call wait() failed...\n");
						exit(4);
					}
					getrusage(RUSAGE_CHILDREN, &rs);
					gettimeofday(&en, NULL);
					if(__SHOW_DETAILS__) {
						printf("PID %d\t[%s] completed.\n", wait_pid, pargs[0]);
						printMessage(&st, &en, &rs);
					}
				}
			}
		}
	}
	
	return 0;
}

void add(Node N) {
	if(!head) {
		head = new Node;
		*head = N;
		head->next = NULL;
		tail = head;
	}
	else {
		tail->next = new Node;
		tail = tail->next;
		*tail = N;
		tail->next = NULL;
	}
}

void del(int pid) {
	Node *temp, *curr;
	if(!head) return;
	if(head->pid == pid) {
		temp = head;
		head = head->next;
		delete temp;
		if(head == NULL) {
			tail = NULL;
			__CURR_BCKGRND__ = 0;
		}
		return;
	}
	curr = head;
	while(curr->next && curr->next->pid == pid) {
		temp = curr->next;
		curr->next = temp->next;
		delete temp;
		return;
	}
}

Node * find(int pid) {
	Node *curr = head;
	while(curr) {
		if(curr->pid == pid) return curr;
		curr = curr->next;
	}
	return NULL;
}

void changeDirectory(char *ptr) {
	char curr[128];
	int ret, i, pos;
	
	getcwd(curr, 128);
	if(!ptr[0]) ret = chdir((const char *)getenv("HOME"));
	else if(ptr[0]=='.' && ptr[1]!='.') {
		strcat(curr, ptr+1);
		ret = chdir((const char *)curr);
	}
	else if(!strcmp(ptr, "..")) {
		for(i = pos = 0; curr[i]; i++) if(curr[i]=='/') pos = i;
		if(!pos) pos++; curr[pos] = 0;
		ret = chdir((const char *)curr);
	}
	else if(ptr[0]=='/') ret = chdir((const char *)ptr);
	else {
		strcat(curr, "/");
		strcat(curr, ptr);
		ret = chdir((const char *)curr);
	}
	if(ret == -1) printf("mShell: %s: No such directory...\n", ptr);
}

void printWorkingDirectory(void) {
	char curr[128];
	puts(getcwd(curr, 128));
}

void getCurrentDirectory(char *s) {
	char curr[128];
	int i, pos;
	getcwd(curr, 128);
	for(i = pos = 0; curr[i]; i++) if(curr[i] =='/') pos = i;
	if(i == 1) strcpy(s, "/");
	else strcpy(s, &curr[pos+1]);
}

void signalHandler(int sig) {
	int status;
	pid_t t_pid;
	rusage rs;
	timeval ts;
	Node *n;
	if(sig == SIGCHLD) {
		t_pid = wait(&status);
		if(t_pid > 0) {
			getrusage(RUSAGE_CHILDREN, &rs);
			gettimeofday(&ts, NULL);
			n = find(t_pid);
			if(n != NULL) {
				n->ru = rs;
				n->en = ts;
				n->done = 0;
			}
		}
	}
	signal(SIGCHLD, signalHandler);
}

void showCompletedJobs(int incR) {
	Node *curr = head, *temp;
	while(curr) {
		temp = curr->next;
		if(! curr->done) {
			printf("[%d] %d\t[%s] completed.\n", curr->sl, curr->pid, curr->name);
			if(__SHOW_DETAILS__) {
				printMessage(&(curr->st), &(curr->en), &(curr->ru));
			}
			del(curr->pid);
		}
		curr = temp;
	}
	if(incR) showRunningJobs();
}

void showRunningJobs() {
	Node *curr = head;
	while(curr) {
		printf("[%d] %d\t[%s] Running.\n", curr->sl, curr->pid, curr->name);
		curr = curr->next;
	}
}

void printMessage(timeval *st, timeval *en, rusage *rs) {
	timeval tu, tv;
	int ist, ien;
	ist = st->tv_sec * 10000000 + st->tv_usec;
	ien = en->tv_sec * 10000000 + en->tv_usec;
	tu = rs->ru_utime;
	tv = rs->ru_stime;
	printf("\n---Process Statistics---\n");
	printf("user time = %.3lf(ms); system time = %.3lf(ms); wallclock time = %.3lf(ms)\n", (tu.tv_sec * 1000000.0 + tu.tv_usec) / 1000.0, (tv.tv_sec * 1000000.0 + tv.tv_usec) / 1000.0, (ien-ist)/1000.0);
	printf("voluntary context switches = %ld; involuntary context switches = %ld\n", rs->ru_nvcsw, rs->ru_nivcsw);
	printf("total page faults = %ld; minor page faults = %ld\n",rs->ru_majflt + rs->ru_minflt, rs->ru_minflt);
	printf("------------------------\n");
}

/* END OF SOURCE CODE */
