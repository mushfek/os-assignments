/**
 * Sample implementation of Linux ls command
 * 
 * @author Mushfekur Rahman
 * @since 1.0
 **/

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <langinfo.h>
#include <locale.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#define DEBUG if(1)
#define max(a,b) ((a)>(b)?(a):(b))

#define SORT_BY_NAME 0
#define SORT_BY_SIZE 1
#define SORT_BY_TIME 2

#define TIME_LAST_MODIFIED 0
#define TIME_LAST_ACCESSED 1
#define TIME_LAST_FLAGCNGD 2

#define SIZE_BYTES 0
#define SIZE_KBYTE 1
#define SIZE_HUMAN 2

#ifndef BLOCKSIZE
	#define BLOCKSIZE 512
#endif

typedef struct dirent DIRENT;
typedef struct stat STAT;
typedef struct passwd PWD;
typedef struct group GRP;
typedef struct tm TM;

struct Data {
	char *name, *actual;
	STAT stat;
};

struct Node {
	Data entry;
	Node *next;
	Node() { next = NULL; }
	~Node() {}
};

struct DLL {
	Node *head, *tail;
	int size;
	DLL() { head = tail = NULL; size = 0; }
	~DLL() {}
};

char **pathv; int npathv;
char **soptv; int nsoptv;
char **loptv; int nloptv;
char cwd[2] = ".";
int pathentry, sortingmode, timeformat, sizeformat, argsort;

/** output formatting settings **/
static int LSOPT_1; // force output one entry per line
static int LSOPT_A; // do not list . and ..
static int LSOPT_a; // list all, overrides LSOPT_A
static int LSOPT_B; // ignore backups
static int LSOPT_c; // use time last flag changed in sorting with -t
static int LSOPT_d; // disable referencing and recursive listing
static int LSOPT_F; // enable file type marking
static int LSOPT_f; // disable sorting, last one overrides others
static int LSOPT_G; // ignore group if long format is used
static int LSOPT_g; // long format without owner entry
static int LSOPT_H; // follow symlink in long format for argument entries
static int LSOPT_h; // human readable size, nearest metric
static int LSOPT_i; // enable inode number in first column
static int LSOPT_k; // killobyte size
static int LSOPT_l; // long format, introduces LSOPT_1 as well
static int LSOPT_m; // comma separated list, last one effective -l -1 -m
static int LSOPT_n; // long format with integer ids
static int LSOPT_o; // long format without group entry
static int LSOPT_Q; // surround names with double quites
static int LSOPT_q; // force nongraphic character to be ?
static int LSOPT_R; // search recursively
static int LSOPT_r; // reverse sorting order
static int LSOPT_S; // sort by size
static int LSOPT_s; // display size
static int LSOPT_t; // sort by type
static int LSOPT_U; // do not sort
static int LSOPT_u; // use time last accessed in sorting with -t
static int LSOPT_w; // force raw formatting
/********************************/

void insert(DLL *, char *);
void erase(DLL *);
void init_display_formats(void);
void traverseDirectory(char *);
int argcomp(const void *, const void *);
int mode(char *);
void errormsg(char *, char *, int);
void printFormatted(DLL *);
void getActualName(char *, char **);
int mystrcmp(char *, char *);
void printMode(STAT *, char *, int);

int main(int argc, char **argv) {
	int i, cs, nxt, md;
	DLL *dlist;
	DIR *dir;
	pathv = new char*[argc];
	soptv = new char*[argc];
	loptv = new char*[argc];
	for(i = 1, pathentry = 0; i < argc; i++) {
		if(!strncmp(argv[i], "--", 2)) loptv[nloptv++] = argv[i];
		else if(argv[i][0]=='-') soptv[nsoptv++] = argv[i];
		else {
			cs = mode(argv[i]); pathentry++;
			if(cs != -1) pathv[npathv++] = argv[i];
		}
	}
	if(!pathentry) {
		pathv[npathv++] = cwd;
		pathentry++;
	}
	init_display_formats();
	argsort = 1;
	qsort(pathv, npathv, sizeof(char *), argcomp);
	argsort = 0;
	dlist = new DLL;
	for(i = 0; i < npathv; i++) {
		md = mode(pathv[i]);
		if(LSOPT_d) {
			insert(dlist, pathv[i]);
			pathv[i] = NULL;
		}
		else if((md & S_IFMT)==S_IFLNK) {
			if(LSOPT_l && !LSOPT_H) {
				insert(dlist, pathv[i]);
				pathv[i] = NULL;
			}
		}
		else if((md & S_IFMT) != S_IFDIR) {
			insert(dlist, pathv[i]);
			pathv[i] = NULL;
		}
	}
	for(cs = 0; cs < npathv; cs++) {
		if(!pathv[cs]) continue;
		if((dir = opendir(pathv[cs])) == NULL) {
			errormsg((char *)"ls: cannot open directory", pathv[cs], errno);
			pathv[cs] = NULL;
		}
		else closedir(dir);
	}
	if(dlist->size) printFormatted(dlist);
	for(i = cs = 0; i < npathv; i++) {
		if(pathv[i]) pathv[cs++] = pathv[i];
	}
	npathv = cs;
	
	qsort(pathv, npathv, sizeof(char *), argcomp);
	for(nxt = dlist->size, i = 0; i < npathv; i++) {
		if(pathv[i]) {
			if(pathentry > 1 || LSOPT_R) {
				if(nxt++) printf("\n");
				if(LSOPT_Q) printf("\"");
				printf("%s", pathv[i]);
				if(LSOPT_Q) printf("\"");
				printf(":\n");
			}
			traverseDirectory(pathv[i]);
		}
	}
	erase(dlist);
	delete pathv;
	delete soptv;
	delete loptv;
	return 0;
}

void init_display_formats() {
	int i, j, len;
	if(!geteuid()) LSOPT_A = 1;
	if(!isatty(fileno(stdout))) LSOPT_1 = LSOPT_w = 1;
	sortingmode = SORT_BY_NAME;
	timeformat = TIME_LAST_MODIFIED;
	sizeformat = SIZE_BYTES;
	for(i = 0; i < nsoptv; i++) {
		len = strlen(soptv[i]);
		for(j = 1; j < len; j++) {
			switch(soptv[i][j]) {
				case '1': if(!LSOPT_l) LSOPT_1 = 1, LSOPT_m = 0; break;
				case 'A': if(!LSOPT_a) LSOPT_A = 1; break;
				case 'a': LSOPT_a = 1, LSOPT_A = 0; break;
				case 'B': LSOPT_B = 1; break;
				case 'c': LSOPT_c = 1, timeformat = TIME_LAST_FLAGCNGD; break;
				case 'd': LSOPT_d = 1, LSOPT_R = LSOPT_H = 0; break;
				case 'F': LSOPT_F = 1; break;
				case 'f': LSOPT_f = LSOPT_a = LSOPT_U = 1, LSOPT_l = LSOPT_s = LSOPT_A = LSOPT_S = LSOPT_t = 0, sortingmode = -1; break;
				case 'G': LSOPT_G = 1; break;
				case 'g': LSOPT_g = LSOPT_l = 1, LSOPT_1 = LSOPT_m = 0; break;
				case 'H': if(!LSOPT_d) LSOPT_H = 1; break;
				case 'h': LSOPT_h = 1, sizeformat = SIZE_HUMAN; break;
				case 'i': LSOPT_i = 1; break;
				case 'k': LSOPT_k = 1, sizeformat = SIZE_KBYTE; break;
				case 'l': LSOPT_l = 1, LSOPT_m = LSOPT_1 = 0; break;
				case 'm': LSOPT_m = 1, LSOPT_l = LSOPT_1 = 0; break;
				case 'n': LSOPT_n = LSOPT_l = 1, LSOPT_1 = LSOPT_m = 0; break;
				case 'o': LSOPT_o = LSOPT_l = 1, LSOPT_1 = LSOPT_m = 0; break;
				case 'Q': LSOPT_Q = 1; break;
				case 'q': LSOPT_q = 1; break;
				case 'R': if(!LSOPT_d) LSOPT_R = 1; break;
				case 'r': LSOPT_r = 1; break;
				case 'S': LSOPT_S = 1, LSOPT_f = LSOPT_U = 0, sortingmode = SORT_BY_SIZE; break;
				case 's': LSOPT_s = 1; break;
				case 't': LSOPT_t = 1, LSOPT_f = LSOPT_U = 0, sortingmode = SORT_BY_TIME; break;
				case 'U': LSOPT_U = 1, LSOPT_S = LSOPT_t = 0, sortingmode = -1; break;
				case 'u': LSOPT_u = 1, timeformat = TIME_LAST_ACCESSED; break;
				case 'w': LSOPT_w = 1; break;
			}
		}
	}
}

void traverseDirectory(char *path) {
	DIR *tmpdir;
	DIRENT *tmpdirent;
	DLL *dlist = new DLL;
	Node *curr;
	char **tmpv, *tmpbuf, *act;
	int ntmpv, i, md, nxt, tmpsz = 0;
	tmpdir = opendir(path);
	while((tmpdirent = readdir(tmpdir)) != NULL) {
		tmpbuf = new char[strlen(path) + strlen(tmpdirent->d_name) + 2];
		strcpy(tmpbuf, path); strcat(tmpbuf, "/"); strcat(tmpbuf, tmpdirent->d_name);
		insert(dlist, tmpbuf);
	}
	closedir(tmpdir);
	tmpv = new char*[dlist->size];
	curr = dlist->head; ntmpv = 0;
	while(curr) {
		tmpv[ntmpv++] = curr->entry.name;
		curr = curr->next;
	}
	erase(dlist);
	dlist = new DLL;
	qsort(tmpv, ntmpv, sizeof(char *), argcomp);
	for(i = 0; i < ntmpv; i++) {
		insert(dlist, tmpv[i]);
		curr = dlist->tail;
		if(!LSOPT_a) {
			if(LSOPT_A && !strcmp(curr->entry.actual, ".")) continue;
			if(LSOPT_A && !strcmp(curr->entry.actual, "..")) continue;
			if(!LSOPT_A && !strncmp(curr->entry.actual, ".", 1)) continue;
		}
		if(LSOPT_B && curr->entry.actual[strlen(curr->entry.actual)-1]=='~') continue;
		tmpsz += curr->entry.stat.st_blocks;
	}
	if(LSOPT_l || LSOPT_s) printf("total %d\n", tmpsz >> 1);
	printFormatted(dlist);
	if(LSOPT_R) {
		for(i = 0; i < ntmpv; i++) {
			getActualName(tmpv[i], &act);
			if(!strcmp(act, ".") || !strcmp(act, "..")) continue;
			nxt = LSOPT_l; LSOPT_l = 1;
			md = mode(tmpv[i]);
			LSOPT_l = nxt;
			if((md & S_IFMT) == S_IFDIR) {
				if((tmpdir = opendir(tmpv[i])) == NULL) {
					errormsg((char *)"ls: cannot open directory", tmpv[i], errno);
				}
				else {
					closedir(tmpdir);
					printf("\n");
					if(LSOPT_Q) printf("\"");
					printf("%s", tmpv[i]);
					if(LSOPT_Q) printf("\"");
					printf(":\n");
					traverseDirectory(tmpv[i]);
				}
			}
		}
	}
	erase(dlist);
	delete tmpv;
}

void printFormatted(DLL *root) {
	Node *curr = root->head;
	char buff[256], ch, deg[] = " KMGT";
	int nxt = 0, len, i;
	double dlen;
	PWD *pwd;
	GRP *grp;
	STAT tmp;
	TM *tm;
	while(curr) {
		if(!LSOPT_a) {
			if(LSOPT_A && !strcmp(curr->entry.actual, ".")) { curr = curr->next; continue; }
			if(LSOPT_A && !strcmp(curr->entry.actual, "..")) { curr = curr->next; continue; }
			if(!LSOPT_A && !strncmp(curr->entry.actual, ".", 1)) { curr = curr->next; continue; }
		}
		if(LSOPT_B && curr->entry.actual[strlen(curr->entry.actual)-1]=='~') { curr = curr->next; continue; }
		//if(((nxt) & 3) == 0) printf("/\n");
		if(nxt++) {
			if(LSOPT_1 || LSOPT_l) printf("\n");
			else {
				printf("    ");
			}
		}
		if(LSOPT_i) printf("%8d ", (int) curr->entry.stat.st_ino);
		if(LSOPT_s) {
			switch(sizeformat) {
				case SIZE_BYTES: printf("%3ld ", (curr->entry.stat.st_size+BLOCKSIZE-1)/BLOCKSIZE); break;
				case SIZE_KBYTE: printf("%3ld ", (curr->entry.stat.st_size+BLOCKSIZE-1)/BLOCKSIZE); break;
				case SIZE_HUMAN: printf("%3ld", (curr->entry.stat.st_size+BLOCKSIZE-1)/BLOCKSIZE);
					if(curr->entry.stat.st_blocks>>1) printf("k"); else printf(" ");
				break;
			}
			printf(" ");
		}
		if(LSOPT_l) printMode(&curr->entry.stat, &ch, 1);
		if(LSOPT_l) printf("%5d", (int) curr->entry.stat.st_nlink);
		if(LSOPT_l && !LSOPT_g) {
			if(!LSOPT_n && (pwd = getpwuid(curr->entry.stat.st_uid)) != NULL) printf("%10s", pwd->pw_name);
			else printf("%6d ", curr->entry.stat.st_uid);
		}
		if(LSOPT_l && !LSOPT_o && !(LSOPT_l && LSOPT_G)) {
			if(!LSOPT_n && (grp = getgrgid(curr->entry.stat.st_gid)) != NULL) printf("%10s", grp->gr_name);
			else printf("%6d ", curr->entry.stat.st_gid);
		}
		if(LSOPT_l) {
			if((curr->entry.stat.st_mode & S_IFMT) == S_IFCHR || (curr->entry.stat.st_mode & S_IFMT) == S_IFBLK) {
				printf(" %3d,%3d ", major(curr->entry.stat.st_rdev), minor(curr->entry.stat.st_rdev));
			}
			else {
				switch(sizeformat) {
					case SIZE_BYTES: printf(" %8d ", (int) curr->entry.stat.st_size); break;
					case SIZE_KBYTE: printf(" %4d ", (int) (curr->entry.stat.st_size+1023)>>10); break;
					case SIZE_HUMAN:
						dlen = curr->entry.stat.st_size; i = 0;
						while(dlen > 1024) {
							dlen /= 1024;
							i++;
						}
						printf(" %8.1lf%c ", dlen, deg[i]);
				}
			}
		}
		if(LSOPT_l) {
			if(timeformat == TIME_LAST_MODIFIED) tm = localtime(&curr->entry.stat.st_mtime);
			else if(timeformat == TIME_LAST_ACCESSED) tm = localtime(&curr->entry.stat.st_atime);
			else if(timeformat == TIME_LAST_FLAGCNGD) tm = localtime(&curr->entry.stat.st_ctime);
			printf("%4d-%02d-%02d %02d:%02d ", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);
		}
		if(LSOPT_Q) printf("\"");
		printf("%s", curr->entry.actual);
		if(LSOPT_Q) printf("\"");
		printf("%c", (LSOPT_m?',':' '));
		if(LSOPT_F) { printMode(&curr->entry.stat, &ch, 0); printf("%c", ch); }
		if(LSOPT_l && (curr->entry.stat.st_mode & S_IFMT) == S_IFLNK) {
			printf(" -> ");
			if((len = readlink(curr->entry.name, buff, 256)) == -1) {
				exit(errno);
			}
			buff[len] = 0;
			if(LSOPT_Q) printf("\""); printf("%s", buff); if(LSOPT_Q) printf("\"");
			if(LSOPT_F) {
				lstat(buff, &tmp);
				printMode(&tmp, &ch, 0);
				printf("%c", ch);
			}
		}
		curr = curr->next;
	}
	printf("\n");
}

int argcomp(const void *a, const void *b) {
	char **x = (char **)a;
	char **y = (char **)b;
	STAT sa, sb;
	int da, db;
	int (*mystat)(const char*, STAT*) = NULL;
	if(argsort) {
		if(LSOPT_l && !LSOPT_H) mystat = &lstat;
		else mystat = &stat;
	}
	else mystat = &lstat;
	da = mystat(*x, &sa);
	db = mystat(*y, &sb);
	assert(da > -1 && db > -1);
	da = ((sa.st_mode & S_IFMT) == S_IFDIR);
	db = ((sb.st_mode & S_IFMT) == S_IFDIR);
	if(argsort && !LSOPT_d && da != db) return da - db;
	if(sortingmode == SORT_BY_NAME) {
		if(LSOPT_r) return mystrcmp(*y, *x);
		return mystrcmp(*x, *y);
	}
	else if(sortingmode == SORT_BY_SIZE) {
		if(LSOPT_r) return sa.st_size - sb.st_size;
		return sb.st_size - sa.st_size;
	}
	else if(sortingmode == SORT_BY_TIME) {
		if(timeformat == TIME_LAST_MODIFIED) {
			if(LSOPT_r) return sa.st_mtime - sb.st_mtime;
			return sb.st_mtime - sa.st_mtime;
		}
		else if(timeformat == TIME_LAST_ACCESSED) {
			if(LSOPT_r) return sa.st_atime - sb.st_atime;
			return sb.st_atime - sa.st_atime;
		}
		else if(timeformat == TIME_LAST_FLAGCNGD) {
			if(LSOPT_r) return sa.st_ctime - sb.st_ctime;
			return sb.st_ctime - sa.st_ctime;
		}
	}
	else return -1;
}

int mode(char *d_name) {
	STAT tmpstat;
	if(lstat(d_name, &tmpstat) == -1) {
		errormsg((char *)"ls: cannot access", d_name, errno);
		return -1;
	}
	return (int) tmpstat.st_mode;
}

void insert(DLL *root, char *name) {
	if(!root) return;
	Data entry;
	int len, i;
	entry.name = new char[strlen(name)+1];
	strcpy(entry.name, name);
	getActualName(entry.name, &entry.actual);
	if(LSOPT_q) {
		len = strlen(entry.actual);
		for(i = 0; i < len; i++) if(entry.actual[i] >= 128) entry.actual[i] = '?';
	}
	lstat(name, &entry.stat);
	root->size++;
	if(root->head == NULL) {
		root->head = new Node();
		root->tail = root->head;
		root->head->entry = entry;
	}
	else {
		root->tail->next = new Node();
		root->tail = root->tail->next;
		root->tail->entry = entry;
	}
}

void erase(DLL *root) {
	Node *temp;
	if(!root) return;
	while(root->head) {
		temp = root->head;
		root->head = root->head->next;
		delete temp;
	}
	delete root;
}

void errormsg(char *pre, char *msg, int err) {
	char buff[128];
	sprintf(buff, "%s %s", pre, msg);
	errno = err;
	perror(buff);
}

void getActualName(char *src, char **dst) {
	int i, len = strlen(src);
	for(i = len-1; i >= 0; i--) {
		if(src[i] == '/') break;
	}
	*dst = &src[i+1];
}

int mystrcmp(char *a, char *b) {
	char *sa, *sb;
	int i, la, lb;
	getActualName(a, &sa);
	getActualName(b, &sb);
	la = strlen(sa);
	lb = strlen(sb);
	if(strcmp(sa,".") && strcmp(sa,"..")) {
		for(i = 0; i < la; i++) if(sa[i] != '.') break;
		if(i < la) sa += i;
	}
	if(strcmp(sb,".") && strcmp(sb,"..")) {
		for(i = 0; i < lb; i++) if(sb[i] != '.') break;
		if(i < lb) sb += i;
	}
	return strcmp(sa, sb);
}

void printMode(STAT *st, char *ch, int print) {
	char sym;
	switch(st->st_mode & S_IFMT) {
		case S_IFBLK:  sym = 'b'; *ch = 0;   break;
		case S_IFCHR:  sym = 'c'; *ch = 0;   break;
		case S_IFDIR:  sym = 'd'; *ch = '/'; break;
		case S_IFIFO:  sym = 'p'; *ch = '|'; break;
		case S_IFLNK:  sym = 'l'; *ch = '@'; break;
		case S_IFREG:  sym = '-'; *ch = 0;   break;
		case S_IFSOCK: sym = 's'; *ch = '='; break;
		default:       sym = 'w'; *ch = '%'; break;
	}
	if((st->st_mode & S_IFMT) == S_IFREG && (st->st_mode & 0111)) *ch = '*';
	if(!print) return;
	printf("%c", sym);
	if(st->st_mode & S_IRUSR) printf("r"); else printf("-");
	if(st->st_mode & S_IWUSR) printf("w"); else printf("-");
	if(!(st->st_mode & S_IXUSR) && (st->st_mode & S_ISUID)) printf("S");
	else if((st->st_mode & S_IXUSR) && (st->st_mode & S_ISUID)) printf("s");
	else if(st->st_mode & S_IXUSR) printf("x");
	else printf("-");
	if(st->st_mode & S_IRGRP) printf("r"); else printf("-");
	if(st->st_mode & S_IWGRP) printf("w"); else printf("-");
	if(!(st->st_mode & S_IXGRP) && (st->st_mode & S_ISGID)) printf("S");
	else if((st->st_mode & S_IXGRP) && (st->st_mode & S_ISGID)) printf("s");
	else if(st->st_mode & S_IXGRP) printf("x");
	else printf("-");
	if(st->st_mode & S_IROTH) printf("r"); else printf("-");
	if(st->st_mode & S_IWOTH) printf("w"); else printf("-");
	if(!(st->st_mode & S_IXOTH) && (st->st_mode & S_ISVTX)) printf("T");
	else if((st->st_mode & S_IXOTH) && (st->st_mode & S_ISVTX)) printf("t");
	else if(st->st_mode & S_IXOTH) printf("x");
	else printf("-");
}

/** end of source code **/
