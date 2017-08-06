/*
 ============================================================================
 Name        : myshell.c
 Author      : tchen
 Version     : 1.0
 Copyright   : Your copyright notice
 Description : Myshell in C, Ansi-style
 ============================================================================
 */

#include "myshell.h"
	
char* internal_cmd[] = {"cd", "bg", "continue", "echo", "exec", "exit", "fg", "jobs", "kill",
				  	"pwd", "set", "shift", "test", "time", "umask", "unset", "environ",
				  	"clr", "myshell", "quit", "help", "more", "mkdir", "mv", "rm", "rmdir",
					"date", "cp", "dir", "history", "atest", "head", "tail", "touch", NULL};

char buffer[MAXLINE];	// 临时数组
char cmdline[MAXLINE];	// 读取命令，命令存放数组
char pathname[MAXSIZE];	// 路径辅助数组
char processname[MAXSIZE];	// 路径辅助数组
char* variable[MAXLINE];	// 普通变量字符串数组
char* environ[MAXLINE];		// 环境变量字符串数组
char* history[MAXSIZE];		// 历史命令字符串数组
static int history_num = 0;	// 历史命令个数
static int environ_num = 0;	// 环境变量个数
static int variable_num = 0;	//普通变量个数
static int para_count = 0;	// 当前脚本传入参数个数
static int is_back = 0;		// 是否是后台运行
static int run_flag = 1;	// 是否继续运行
pid_t son_pid = 0;			// 子进程的进程号
struct winsize size;		// 屏幕大小
typedef struct job {		// 后台运行命令数组
	pid_t pid;
	char state[8];
	char cmd[20];
	struct job *next;
}job;
job *head, *tail;			// 链表头，尾

/* 申请一块干净的内存（所有字节都是'\0'） */
char* malloc_clear(int len) {
	char* ret = (char*)malloc(len);
	memset(ret, '\0', len);
	return ret;
}

/* 把一个命令分割成多个部分
   比如, "echo aa bb" 会被分割成 "echo" "aa" "bb" 和 NULL */
char** splitline(char* cmd) {
	if (cmd[0] == '\0' || cmd == NULL)
		return NULL;
	/* 将命令保存到历史数组里 */
	history[history_num++] = strdup(cmd);
	char** ret;
	char* cur = cmd;
	char* left;
	int count = 0;
	if ((ret = malloc(MAXLINE)) == NULL) {
		perror("split");
		return NULL;
	}
	/* 根据空格符来分割命令 */
	while (*cur != '\0') {
		while (*cur == ' ' || *cur == '\t')
			cur++;
		if (*cur == '\0')
			break;
		else {
			int len = 0;
			left = cur;
			while (!(*cur == ' ' || *cur == '\t') && *cur != '\0') {
				cur++;
				len++;
			}
			char* t = malloc_clear(len + 1);
			strncpy(t, left, len);
			t[len] = '\0';
			ret[count] = t;
			count++;
		}
	}
	ret[count] = NULL;
	return ret;
}

/* 读取一个命令，并且最后返回它分割之后的字符串数组 */
char** read_command() {
	memset(cmdline, '\0', MAXLINE);
	if (fgets(cmdline, MAXLINE, stdin) == 0) {
		printf("\n");
		exit(0);
	}
	cmdline[strlen(&cmdline[0]) - 1] = '\0';
	/* 将读取到的命令行直接分割 */
	return (splitline(cmdline));
}

/* 判断某一个字符串是否在一个字符串数组里，主要用来判断内部命令 */
int in(char* a, char* b[], int n) {
	int i;
	for (i = 0; i < n; i++) {
		if (!strcmp(a, b[i]))
			return (i + 1);
	}
	return 0;
}

/* 计算命令的长度，即argc */
int length(char** cmd) {
	int count = 0;
	while (cmd[count] != NULL)
		count++;
	return count;
}

/* 得到当前myshell的运行路径，而非工作路径 */
char* get_path() {
	memset(pathname, '\0', MAXSIZE);
	memset(processname, '\0', MAXSIZE);
	char* path_end;
	/* 读取当前的运行路径 */
	if (readlink("/proc/self/exe", pathname, MAXSIZE) <= 0)
		return NULL;
	/* 通过'/'来判断位置 */
	path_end = strrchr(pathname, '/');
	if (path_end == NULL)
		return NULL;
	path_end++;
	strcpy(processname, path_end);
	*path_end = '\0';
	char* ret = malloc_clear(strlen(pathname) + strlen(processname) + 1);
	strcat(ret, pathname);
	strcat(ret, processname);
	return ret;
}

/* 得到用来显示的路径，比如 "myshell:~/$ " */
char* get_display_path() {
	char* home = getenv("HOME");
	memset(buffer, 0, MAXLINE);
	getcwd(buffer, MAXLINE);
	char* curdir = malloc_clear(strlen(buffer) + 1);
	strcpy(curdir, buffer);
	/* 如果显示路径前面部分与主目录相同，用'~'来替代 */
	if (strncmp(curdir, home, strlen(home)) == 0) {
		char* disdir = malloc_clear(strlen(buffer) - strlen(home) + 1);
		strcpy(disdir, curdir + strlen(home));
		free(curdir);
		curdir = malloc_clear(strlen(buffer) - strlen(home) + 2);
		strcat(curdir, "~");
		strcat(curdir, disdir);
		free(disdir);
	}
	/* 为显示路径加上"myshell:", "$ "等提示符 */
	char* retdir = malloc_clear(strlen(curdir) + strlen("myshell:") + strlen("$ ") + 1);
	strcat(retdir, "myshell:");
	strcat(retdir, curdir);
	strcat(retdir, "$ ");
	free(curdir);
	return retdir;
}

/* 设置环境变量，设置的格式是 "name=path" */
void set_path(char* name, char* path) {
	environ[environ_num] = malloc_clear(strlen(name) + strlen(path) + 2);
	strcat(environ[environ_num], name);
	strcat(environ[environ_num], "=");
	strcat(environ[environ_num], path);
	environ_num++;
}

/* 初始化所有的环境变量和工作路径 */
void init() {
	head = tail = NULL;
	char* shell = get_path();
	char* home = getenv("HOME");
	/* 初始化"HOME", "PWD", "shell"环境变量 */
	set_path("HOME", home);
	set_path("PWD", home);
	set_path("shell", shell);
	free(shell);
	chdir(home);
}

/* 判断是否是内部命令，通过in这个辅助函数来检验 */
int is_internal_cmd(char** cmd) {
	if (cmd == NULL)
		return 0;
	else
		return(in(cmd[0], internal_cmd, length(internal_cmd)));
}

/* 判断是否是管道命令，即是否含"|" */
int is_pipe(char** cmd) {
	if (cmd == NULL)
		return 0;
	int i = 0;
	while (cmd[i] != NULL) {
		/* 寻找命令中的'|'符号 */
		if (strchr(cmd[i], '|') != NULL)
			return 1;
		i++;
	}
	return 0;
}

/* 判断是否是重定向命令，即是否含有 "<", ">", "">>" */
int is_io_redirect(char** cmd) {
	if (cmd == NULL)
		return 0;
	int i = 0;
	while (cmd[i] != NULL) {
		/* 寻找命令中的"<", ">", ">>"符号 */
		if (strstr(cmd[i], "<") ||
			strstr(cmd[i], ">") ||
			strstr(cmd[i], ">>"))
			return 1;
		i++;
	}
	return 0;
}

/* 如果一个命令不满足上述三个条件，即为纯粹的外部命令 */
int is_normal(char** cmd) {
	return (!(is_internal_cmd(cmd) || is_io_redirect(cmd) || is_pipe(cmd)));
}

/* 如果一个变量的格式是"$x"，那么就用它的值来代替它本身 */
void my_convert(char* x) {
	if (x[0] == '$') {
		/* 首先遍历所有的普通变量 */
		char** p = variable;
		while (*p != NULL) {
			char* tmp = malloc_clear(strlen(*p) + 2);
			strcat(tmp, "$");
			strcat(tmp, *p);
			int comp_len = strlen(x);
			/* 比较左值是否相同，即变量名是否相同 */
			if (strncmp(tmp, x, comp_len) == 0) {
				free(x);
				x = malloc_clear(strlen(tmp + comp_len + 1) + 1);
				strcpy(x, tmp + comp_len + 1);
				free(tmp);
				return;
			}
			free(tmp);
			p++;
		}
		/* 遍历所有的环境变量 */
		p = environ;
		while (*p != NULL) {
			char* tmp = malloc_clear(strlen(*p) + 2);
			strcat(tmp, "$");
			strcat(tmp, *p);
			int comp_len = strlen(x);
			if (strncmp(tmp, x, comp_len) == 0) {
				free(x);
				x = malloc_clear(strlen(tmp + comp_len + 1) + 1);
				strcpy(x, tmp + comp_len + 1);
				free(tmp);
				return;
			}
			free(tmp);
			p++;
		}
	}
}

/* 设置普通变量，字符串输入的格式是"a=b" */
void my_variable(char* s) {
	unsigned char tmp;
	char *l, *r, *left, *right;
	l = strchr(s, '=');
	right = strdup(l + 1);
	tmp = (unsigned char)(strchr(s, '=') - s);
	r = (tmp > 0) ? strndup(s, tmp) : strdup(s);
	left = r;
	my_convert(right);
	char** p = variable;
	/* 如果这个变量已经存在，那么更新它的值 */
	while (*p != NULL) {
		if (strncmp(s, *p, strlen(left)) == 0) {
			free(*p);
			*p = malloc_clear(strlen(left) + strlen(right) + 2);
			strcat(*p, left);
			strcat(*p, "=");
			strcat(*p, right);
			break;
		}
		p++;
	}
	/* 如果变量不存在，那么就简单插入 */
	if (*p == NULL) {
		variable[variable_num] = malloc_clear(strlen(left) + strlen(right) + 2);
		strcat(variable[variable_num], left);
		strcat(variable[variable_num], "=");
		strcat(variable[variable_num], right);
		variable_num++;
	}
	free(left);
	free(right);
}

/* 循环普通变量数组，输出所有普通变量 */
void my_set() {
	char** t = variable;
	printf("variable=%p\n", t);
	while (*t != NULL) {
		printf("%s\n", *t);
		t++;
	}
}

/* 输入变量名，如果存在，删除该变量 */
void my_unset(char* x) {
	if (x == NULL)
		return;
	int flag = 0;
	char** t = variable;
	while (*t != NULL) {
		unsigned char tmp;
		char *q;
		char *left;
		/* 根据等号分割成两个部分 */
		tmp = (unsigned char)(strchr(*t, '=') - *t);
		q = (tmp > 0) ? strndup(*t, tmp) : strdup(*t);
		left = q;
		if (strcmp(left, x) == 0) {
			flag = 1;
			variable_num--;
			char** s = t + 1;
			while (*s != NULL)
				s++;
			s--;
			/* 如果该变量在变量数组最后一个，直接删除 */
			if (s == t) {
				free(*t);
				*t = NULL;
			}
			/* 如果变量不是数组最后一个，用末尾替代，并删除末尾 */
			else {
				free(*t);
				*t = malloc_clear(strlen(*s) + 1);
				strcpy(*t, *s);
				free(*s);
				*s = NULL;
			}
			break;
		}
		t++;
	}
	/* 若该变量不存在，删除失败 */
	if (!flag && *t == NULL) {
		printf("unset: no such variable\n");
	}
}

/* 将变量"a"的值设置为"b" */
void my_para(char* a, char* b) {
	char* vari = malloc_clear(strlen(a) + strlen(b) + 1);
	/* 形成输入字符串 */
	strcat(vari, a);
	strcat(vari, "=");
	strcat(vari, b);
	my_variable(vari);
	free(vari);
}

/* 设置脚本参数, 比如s #, *, 0, 1, ……
   @符号暂时不支持，因为它还要表示多个参数个数 */
void my_paras(char** cmd) {
	int argc = length(cmd);
	char** argv = cmd;
	int i = 2;
	int total = 0;
	/* 设置变量"1", "2", …… */
	while (argv[i] != NULL) {
		total += strlen(argv[i]);
		para_count++;
		memset(buffer, '\0', MAXLINE);
		sprintf(buffer, "%d", para_count);
		my_para(buffer, argv[i]);
		i++;
	}
	total += para_count - 1;
	memset(buffer, '\0', MAXLINE);
	sprintf(buffer, "%d", para_count);
	/* 设置变量"#" */
	my_para("#", buffer);
	/* 设置变量"0" */
	my_para("0", argv[1]);
	memset(buffer, '\0', MAXLINE);
	sprintf(buffer, "%d", getpid());
	/* 设置变量"$" */
	my_para("$", buffer);

	char* all = malloc_clear(total + 1);
	i = 2;
	/* 拼接形成变量"*"的值 */
	while (argv[i] != NULL) {
		strcat(all, argv[i]);
		strcat(all, " ");
		i++;
	}
	/* 设置变量"*" */
	my_para("*", all);
	free(all);
}

/* 输出普通字符串或是变量 */
void my_echo(char** cmd) {
	int argc = length(cmd);
	char** argv = cmd;
	int i = 1;
	while (argv[i] != NULL) {
		if (argv[i][0] == '$') {
			/* 如果是变量，用它的值替代它本身 */
			my_convert(argv[i]);
			if (argv[i][0] == '$') {
				fprintf(stdout, "%s", "");
			}
			else {
				fprintf(stdout, "%s", argv[i]);
			}
		}
		else
			fprintf(stdout, "%s", argv[i]);
		/* 除了最后一个输出之外，其余之间有空格 */
		if (argv[i + 1] != NULL)
			fprintf(stdout, "%s", " ");
		i++;
	}
	fprintf(stdout, "\n");
}

/* 退出脚本，并且设置"?"的值为退出状态 */ 
void my_exit(char** cmd) {
	int argc = length(cmd);
	char** argv = cmd;
	/* 非法参数个数 */
	if (argc != 1 && argc != 2) {
		printf("exit: invalid number of arguments\n");
		return;
	}
	int status = ((argc == 2) ? atoi(cmd[1]) : 0);
	memset(buffer, '\0', MAXLINE);
	sprintf(buffer, "%d", status);
	my_para("?", buffer);
	exit(status);
}

/* 进入某一个目录，如果未给定回到主目录 */
void my_cd(char** cmd) {
	int argc = length(cmd);
	char** argv = cmd;
	/* 没有参数，回到主目录 */
	if (argc == 1) {
		char* home = getenv("HOME");
		chdir(home);
	}
	/* 进入给定目录 */
	if (argc == 2) {
		if (chdir(argv[1]) < 0)
			printf("cd %s: no such file or directory\n", argv[1]);
	}
}

/* "head"辅助函数，显示文件中前n行 */
void display_head(int n, char* filename) {
	FILE *file;
	int i;
	if (!access(filename, F_OK)) {
		if ((file = fopen(filename, "r")) != 0) {
			printf("%s\n", filename);
			/* 读取文件前n行 */
			for (i = 0; i < n; i++) {
				if (fgets(buffer, MAXLINE, file)) {
					printf("%s", buffer);
				}
				else {
					printf("head: no such number of rows\n");
					break;
				}
			}
			fclose(file);
		}
		else
			printf("head: no such file\n");
	}
	else
		printf("head: no rights\n");
}

/* 显示文件中前若干行 */
void my_head(char** cmd) {
	int argc = length(cmd);
	char** argv = cmd;
	int n;
	if (argc < 2) {
		printf("head: invalid number of arguments\n");
		return;
	}
	int i = 0;
	memset(buffer, '\0', MAXLINE);
	strcpy(buffer, argv[1]);
	int flag = 1;
	/* 判断第二个参数是否存在，是否为纯数字 */
	while (buffer[i] != '\0') {
		if (buffer[i] < '0' || buffer[i] > '9') {
			flag = 0;
			break;
		}
		i++;
	}
	/* 如果有参数输入，那么就显示该参数行 */
	if (flag) {
		n = atoi(argv[1]);
		for (i = 2; i < argc; i++) {
			display_head(n, argv[i]);
			if (i + 1 != argc) {
				printf("\n");
			}
		}
	}
	/* 如果没有参数输入，那么就默认显示前10行 */
	else {
		for (i = 1; i < argc; i++) {
			display_head(10, argv[i]);
			if (i + 1 != argc) {
				printf("\n");
			}
		}
	}
}

/* "tail"辅助函数，显示文件的最后n行 */
void display_tail(int n, char* filename) {
	FILE* file;
	file = fopen(filename, "r");
	if (file == NULL) {
		printf("tail: no such file\n");
		return;
	}
	int pos, index, end;
	long count = 0;
	char c;
	/* 定位到文件末尾 */
	fseek(file, 0, SEEK_END);
	pos = ftell(file);
	end = pos;
	/* 根据'\n'符号不断向前，直到达到n行 */
	for (; pos >= 0; pos--) {
		c = fgetc(file);
		if (c == '\n' && (end - pos) > 1) {
			count++;
			if (count == n + 1) {
				break;
			}
		}
		fseek(file, pos, SEEK_SET);
	}
	fseek(file, (pos + 2), SEEK_SET);
	int i = 0;
	/* 根据获得的位置，输出 */
	for (i = 0; i < n; i++) {
		if (fgets(buffer, MAXLINE, file)) {
			printf("%s", buffer);
		}
		else {
			printf("head: no such number of rows\n");
			break;
		}
	}
	fclose(file);
}

/* 显示文件末尾若干行 */
void my_tail(char** cmd) {
	int argc = length(cmd);
	char** argv = cmd;
	FILE *file;
	if (argc < 2) {
		printf("tail: invalid number of arguments\n");
		return;
	}
	int i = 0;
	memset(buffer, '\0', MAXLINE);
	strcpy(buffer, argv[1]);
	int flag = 1;
	/* 判断是否有参数输入，是否是纯数字 */
	while (buffer[i] != '\0') {
		if (buffer[i] < '0' || buffer[i] > '9') {
			flag = 0;
			break;
		}
		i++;
	}
	/* 如果有参数输入，那么就显示最后参数行 */
	if (flag) {
		int n = atoi(argv[1]);
		for (i = 2; i < argc; i++) {
			printf("%s\n", argv[i]);
			display_tail(n, argv[i]);
			if (i + 1 != argc) {
				printf("\n");
			}
		}
	}
	/* 如果没有参数输入，那么就显示最后10行 */
	else {
		for (i = 1; i < argc; i++) {
			printf("%s\n", argv[i]);
			display_tail(10, argv[i]);
			if (i + 1 != argc) {
				printf("\n");
			}
		}
	}
}

/* 新建文件 */
void my_touch(char** cmd) {
	int argc = length(cmd);
	char** argv = cmd;
	int fd;
	if (argc < 2) {
		printf("touch: invalid number of arguments\n");
		return;
	}
	int i;
	for (i = 1; i < argc; i++) {
		/* 如果文件已经存在，那么更新它的修改时间和创建时间 */
		if (!access(argv[i], F_OK)) {
			if (utime(argv[i], NULL) == 0)
				printf("touch: %s updated\n", argv[i]);
		}
		/* 如果文件不存在，那么就直接创建文件 */
		else {
			fd = open(argv[i], O_CREAT | O_RDWR, 0777);
			if (fd > 0)
				printf("touch: %s created\n", argv[i]);
			else
				perror("touch: fail to create\n");
		}
	}
}

/* 删除文件 */
void my_rm(char** cmd) {
	int argc = length(cmd);
	char** argv = cmd;
	if (argc == 1) {
		printf("rm: invalid number of arguments\n");
		return;
	}
	/* 删除文件 */
	if (!access(argv[1], F_OK)) {
		if (unlink(argv[1])) {
			perror("unlink");
		}
	}
	else
		perror("access");
	return;
}

/* 删除目录 */
void my_rmdir(char** cmd) {
	int argc = length(cmd);
	char **argv = cmd;
	if (argc != 2) {
		printf("rmdir: invalid number of arguments.\n");
		return;
	}
	/* 删除目录 */
	if (!rmdir(argv[1])) {
		printf("rmdir: %s was removed\n", argv[1]);
	}
	else
		perror("rmdir");
	return;
}

/* 新建目录 */
void my_mkdir(char** cmd) {
	int argc = length(cmd);
	char ** argv = cmd;
	if (argc != 2) {
		printf("mkdir: invalid number of arguments\n");
		return;
	}
	/* 新建目录，并提示创建成功 */
	if (!mkdir(argv[1], 0775))
		printf("mkdir: %s created\n", argv[1]);
	else
		perror("mkdir");
}

/* 拷贝文件 */
void my_cp(char** cmd) {
	int argc = length(cmd);
	char **argv = cmd;
	int fd1, fd2, count;
	if (argc != 3) {
		printf("cp: invalid number of arguments\n");
		return;
	}
	/* 从文件1中读取，并且不断写到文件2中 */
	if (!access(argv[1], F_OK)) {
		fd1 = open(argv[1], O_RDONLY);
		if (fd1 > 0) {
			fd2 = open(argv[2], O_CREAT | O_WRONLY, 0777);
			if (fd2 > 0) {
				while ((count = read(fd1, buffer, sizeof(buffer))) > 0)
					write(fd2, buffer, count);
				close(fd2);
				close(fd1);
			}
			else {
				perror("open");
			}
		}
		else {
			perror("open");
		}
	}
	else {
		perror("access");
	}
}

/* 显示当前的日期与时间 */
void my_date() {
	long now, time();
	char *ctime();
	time(&now);
	printf("%s", ctime(&now));
}

/* 移动文件 */
void my_mv(char** cmd) {
	int argc = length(cmd);
	char **argv = cmd;
	if (argc != 3) {
		printf("mv: invalid number of arguments\n");
		return;
	}
	if (access(argv[2], F_OK) == 0) {
		printf("mv: file %s exists", argv[2]);
		return;
	}
	/* 移动操作 */
	else {
		if (!link(argv[1], argv[2])) {
			if (unlink(argv[1])) {
				perror("unlink");
			}
		}
		else {
			perror("link");
		}
	}
	return;
}

/* 显示后台所有任务 */
void my_job() {
	job* p = head;
	int count = 1;
	if (head != NULL) {
		do {
			printf("[%d]\t%d\t%s\t%s", count++, p->pid, p->state, p->cmd);
			printf("\n");
			p = p->next;
		} while (p != NULL);
	}
	else
		printf("jobs: no job\n");
}

/* 添加任务到链表 */
void add_job(job* x) {
	x->next = NULL;
	/* 如果是第一个插入的指针 */
	if (head == NULL) {
		head = x;
		tail = x;
	}
	else {
		tail->next = x;
		tail = x;
	}
}

/* 从链表删除任务 */
void del_job(job* x) {
	job *p, *q;
	int pid = x->pid;
	p = q = head;
	if (head == NULL)
		return;
	while (p->pid != pid && p->next != NULL)
		p = p->next;
	if (p->pid != pid)
		return;
	/* 如果删除的是头指针 */
	if (p == head)
		head = head->next;
	if (p == tail)
		tail = tail->next;
	else {
		while (q->next != p)
			q = q->next;
		if (p == tail) {
			tail = q;
			q->next = NULL;
		}
		else
			q->next = p->next;
	}
	free(p);
}

/* 挂进子进程 */
void my_ctrlz() {
	job *p;
	int i = 1;
	/* 如果是父进程，直接跳过 */
	if (son_pid == 0) {
		return;
	}
	if (head != NULL) {
		p = head;
		while (p->pid != son_pid && p->next != NULL)
			p = p->next;
		if (p->pid == son_pid) {
			strcpy(p->state, "stopped");
		}
		else {
			/* 新建任务指针 */
			p = (job*)malloc(sizeof(job));
			strcpy(p->state, "stopped");
			strcpy(p->cmd, history[history_num - 1]);
			p->pid = son_pid;
			add_job(p);
		}
	}
	else {
		p = (job*)malloc(sizeof(job));
		strcpy(p->state, "stopped");
		strcpy(p->cmd, history[history_num - 1]);
		p->pid = son_pid;
		add_job(p);
	}
	/* 转换进程状态 */
	kill(son_pid, SIGSTOP);
	for (p = head; p->pid != son_pid; p = p->next)
		i++;
	/* 打印当前信息 */
	printf("\n[%d]\t%s\t%s\n", i, tail->state, tail->cmd);
	son_pid = 0;
	return;
}

/* 处理信号函数 */
void sig_handler(int p) {
	if (p == SIGINT) {
		/* 使运行过程中的ctrl+c信号失效 */
		if (son_pid != 0)
			kill(son_pid, SIGTERM);
	}
	else if (p == SIGTSTP) {
		/* 处理ctrl+z的信号函数 */
		my_ctrlz();
	}
}

/* 把某一个具体的任务移到后台执行 */
void my_bg(char** cmd) {
	int argc = length(cmd);
	char** argv = cmd;
	if (argv[1] == NULL) {
		printf("bg: no such task\n");
		return;
	}
	int job_num = atoi(argv[1]);
	int i;
	job* p = head;
	/* 根据指针寻找到对应的任务 */
	for (i = 1; i < job_num && p != NULL; i++)
		p = p->next;
	if (i != job_num) {
		printf("bg: out of range\n");
		return;
	}
	/* 更换进程状态 */
	kill(p->pid, SIGCONT);
	strcpy(p->state, "running");
}

/* 把某一个具体的任务移动到前台执行 */
void my_fg(char** cmd) {
	sigprocmask(SIG_UNBLOCK, &son_set, NULL);
	int argc = length(cmd);
	char** argv = cmd;
	if (argv[1] == NULL) {
		printf("fg: no such task\n");
		return;
	}
	int job_num = atoi(argv[1]);
	int i;
	job* p = head;
	/* 根据指针寻找到具体的任务 */
	for (i = 1; i < job_num && p != NULL; i++)
		p = p->next;
	if (i != job_num) {
		printf("fg: out of range\n");
		return;
	}
	strcpy(p->state, "running");
	son_pid = p->pid;
	/* 更换进程状态 */
	kill(p->pid, SIGCONT);
	del_job(p);
	waitpid(p->pid, NULL, 0);
}

/* 杀死某个具体的进程 */
void my_kill(char** cmd) {
	int argc = length(cmd);
	char** argv = cmd;
	if (argv[1] == NULL) {
		printf("kill: no such task\n");
		return;
	}
	pid_t pid = atoi(argv[1]);
	job* p = head;
	while (p != NULL) {
		if (p->pid == pid)
			break;
		p = p->next;
	}
	/* 如果其在后台运行，删除链表中其指针 */
	if (p != NULL) {
		del_job(p);
	}
	/* 杀死进程 */
	kill(pid, SIGKILL);
}

/* 得到umask值 */
mode_t get_mode() {
	mode_t ret;
	ret = umask(0);
	umask(ret);
	return ret;
}

/* 将umask值转换成字符串 */
void mode_to_str(mode_t mode, char* ret, size_t ret_size) {
	int i;
	const int min_size = 5;
	if (ret == NULL || ret_size < min_size)
		return;
	/* 因为最多有9位，超过9位则越界 */
	for (i = 10; i < 32; ++i) {
		if ((1 << i) & mode) {
			fprintf(stderr, "umask: mode out of range\n");
			return;
		}
	}
	/* 形成返回字符串 */
	snprintf(ret, 2, "%d", mode >> 9);
	for (i = 1; i <= 3; ++i)
		snprintf(ret + i, 2, "%d", 7 & (mode >> (9 - i * 3)));
	ret[i] = 0;
	return;
}

/* 将字符串转换成umask值 */
int str_to_mode(mode_t *mode, char *str) {
	int i, s;
	int str_len = strlen(str);
	/* 非法输入情况检验 */
	if (str_len > 4) {
		fprintf(stderr, "umask: str too long\n");
		return 0;
	}
	for (i = 0; i < str_len; i++) {
		if (str[i] > '7' || str[i] < '0') {
			fprintf(stderr, "umask: str out of range\n");
			return 0;
		}
	}
	int t = atoi(str);
	if (t > 1777) {
		fprintf(stderr, "umask: mode out of range\n");
		return 0;
	}
	/* 形成四位umask值 */
	memset(mode, 0, sizeof(mode_t));
	for (i = 0; i < 4; ++i) {
		s = t % 10;
		t = t / 10;
		*mode = (*mode) | (s << (i * 3));
	}
	return 1;
}

/* 显示或设置umask值 */
void my_umask(char** cmd) {
	int argc = length(cmd);
	char **argv = cmd;
	mode_t m;
	char t[8] = { 0 };
	if (argc != 1 && argc != 2) {
		printf("umask: invalid number of arguments\n");
		return;
	}
	/* 如果没有参数，则显示当前的umask值 */
	if (argc == 1) {
		m = get_mode();
		mode_to_str(get_mode(), t, 8);
		printf("%s\n", t);
	}
	/* 如果有参数，则设置当前的umask值 */
	else if (argc == 2) {
		if (str_to_mode(&m, argv[1]) == 0)
			return;
		else {
			umask(m);
		}
	}
}

/* 显示当前的工作目录 */
char* my_pwd() {
	memset(buffer, 0, sizeof(buffer));
	getcwd(buffer, sizeof(buffer));
	printf("%s\n", buffer);
	return buffer;
}

/* 显示当前的历史指令 */
void my_history() {
	int i = 0;
	for (i = 0; i < history_num; i++) {
		printf("[%d]\t%s\n", i + 1, history[i]);
	}
}

/* 得到显示终端的大小 */
static void get_winsize(int fd) {
	if (ioctl(1, TIOCGWINSZ, (char*)&size) < 0)
		perror("more");
}

/* 如果终端显示大小发生变化，重新读取 */
static void sig_winch(int signo) {
	get_winsize(1);
}

/* "more"辅助函数 */
void none_print(int flag) {
	struct termios init_setting;
	struct termios pend_setting;
	if (tcgetattr(1, &init_setting) < 0)
		return;
	pend_setting = init_setting;
	if (flag == 1)
		pend_setting.c_lflag &= ~ECHO;
	else
		pend_setting.c_lflag |= ECHO;
	tcsetattr(1, TCSANOW, &pend_setting);
}

/* 屏蔽掉其他键，q表示quit，空格表示翻页，回车表示换行 */
int see_more(FILE *cmd) {
	int c;
	system("stty -F /dev/tty cbreak");
	printf("\033[7m more?\033[m");
	none_print(1);
	while ((c = getc(cmd)) != EOF)
	{
		if (c == 'q') {
			printf("\n");
			return 0;
		}
		if (c == ' ') {
			printf("\n");
			return size.ws_row;
		}
		if (c == '\n') {
			printf("\033[7D\033[K");
			return 1;
		}
	}
	system("stty -F /dev/tty -cbreak");
	return 0;
}

/* 输出内容到终端 */
void do_more(FILE *fp) {
	char line[MAXLINE];
	int line_num = 0;
	int reply;
	FILE *fp_tty;
	fp_tty = fopen("/dev/tty", "r");
	if (fp_tty == NULL)
		return;
	while (fgets(line, MAXLINE, fp)) {
		/* 不断读取内容，并且根据键盘反馈决定输出 */
		if (line_num == (size.ws_row - 2)) {
			reply = see_more(fp_tty);
			none_print(0);
			if (reply == 0)
				break;
			line_num -= reply;
		}
		if (fputs(line, stdout) == EOF)
			exit(0);
		line_num++;
	}
	fclose(fp_tty);
}

/* 显示终端屏幕大小的文本内容 */
void my_more(char** cmd) {
	int argc = length(cmd);
	char **argv = cmd;
	FILE *fp;
	if (signal(SIGWINCH, sig_winch) == SIG_ERR)
		perror("more");
	get_winsize(1);
	if (argc == 1)
		do_more(stdin);
	else {
		/* 依次对参数的文档进行"more"操作 */
		while (--argc) {
			if ((fp = fopen(*++argv, "r")) != NULL) {
				do_more(fp);
				fclose(fp);
			}
			else {
				return;
			}
		}
	}
	/* 恢复终端 */
	system("stty -F /dev/tty -cbreak");
}


/* 测试逻辑单项逻辑表达式 */
int single_test(char** cmd) {
	char* symbol = cmd[0];
	char* test = cmd[1];
	struct stat stat_buf;
	/*  非空串 (如果字符串长度不为0) */
	if (strcmp(symbol, "-n") == 0)
		return (strlen(test) != 0);
	/*  空串 (如果字符串长度为0) */
	if (strcmp(symbol, "-z") == 0)
		return (strlen(test) == 0);
	/* 如果文件存在，且该文件是区域设备文件 */
	if (strcmp(symbol, "-b") == 0)
		return (stat(test, &stat_buf) == 0 && S_ISBLK(stat_buf.st_mode));
	/* 当file存在并且是字符设备文件时返回真 */
	if (strcmp(symbol, "-c") == 0)
		return (stat(test, &stat_buf) == 0 && S_ISCHR(stat_buf.st_mode));
	/* 当pathname存在并且是一个目录时返回真 */
	if (strcmp(symbol, "-d") == 0)
		return (stat(test, &stat_buf) == 0 && S_ISDIR(stat_buf.st_mode));
	/* 当pathname指定的文件或目录存在时返回真 */
	if (strcmp(symbol, "-e") == 0)
		return (stat(test, &stat_buf) == 0);
	/* 当file存在并且是普通文件时返回真 */
	if (strcmp(symbol, "-f") == 0)
		return (stat(test, &stat_buf) == 0 && S_ISREG(stat_buf.st_mode));
	/* 测试指定的文件或目录存在并且设置了SGID位 */
	if (strcmp(symbol, "-g") == 0)
		return (stat(test, &stat_buf) == 0 && (stat_buf.st_mode & S_ISGID));
	/* 文件存在，且该文件为有效的群组id所拥有 */
	if (strcmp(symbol, "-G") == 0)
		return (stat(test, &stat_buf) == 0 && (getegid() == stat_buf.st_gid));
	/* 当file存在并且是符号链接文件时返回真，该选项在一些老系统上无效 */
	if (strcmp(symbol, "-h") == 0)
		return (lstat(test, &stat_buf) == 0 && S_ISLNK(stat_buf.st_mode));
	/* 当由pathname指定的文件或目录存在并且设置了“sticky”位时返回真 */
	if (strcmp(symbol, "-k") == 0)
		return (stat(test, &stat_buf) == 0 && (stat_buf.st_mode & S_ISVTX));
	/* 如果该文件存在，且该文件是符号链接文件 */
	if (strcmp(symbol, "-L") == 0)
		return (lstat(test, &stat_buf) == 0 && S_ISLNK(stat_buf.st_mode));
	/* 当由pathname指定的文件或目录存在并且被子当前进程的有效用户ID所指定的用户拥有时返回真 */
	if (strcmp(symbol, "-O") == 0)
		return (stat(test, &stat_buf) == 0 && (geteuid() == stat_buf.st_uid));
	/* 当文件存在并且是管道文件时返回真 */
	if (strcmp(symbol, "-p") == 0)
		return (stat(test, &stat_buf) == 0 && S_ISFIFO(stat_buf.st_mode));
	/* 当由pathname指定的文件或目录存在并且可读时返回为真 */
	if (strcmp(symbol, "-r") == 0)
		return access(test, 4) == 0;
	/* 当file存在文件大小大于0时返回真 */
	if (strcmp(symbol, "-s") == 0)
		return (stat(test, &stat_buf) == 0 && 0 < stat_buf.st_size);
	/* 如果该文件存在，且该文件是Socket文件 */
	if (strcmp(symbol, "-S") == 0)
		return (stat(test, &stat_buf) == 0 && S_ISSOCK(stat_buf.st_mode));
	/* 如果文件描述符是开启的，且链接了某一个终端 */
	if (strcmp(symbol, "-t") == 0) {
		long int fd;
		errno = 0;
		fd = strtol(test, NULL, 10);
		return (errno != ERANGE && 0 <= fd && fd <= INT_MAX && isatty(fd));
	}
	/* 当由pathname指定的文件或目录存在并且设置了SUID位时返回真 */
	if (strcmp(symbol, "-u") == 0)
		return (stat(test, &stat_buf) == 0 && (stat_buf.st_mode & S_ISUID));
	/* 当由pathname指定的文件或目录存在并且可执行时返回真 */
	if (strcmp(symbol, "-w") == 0)
		return access(test, 2) == 0;
	/* 如果文件存在，且该文件有可执行的属性 */
	if (strcmp(symbol, "-x") == 0)
		return access(test, 1) == 0;
	return 0;
}

/* 测试逻辑双项表达式 */
int double_test(char** cmd) {
	char* symbol = cmd[1];
	char* test1 = cmd[0];
	char* test2 = cmd[2];
	struct stat stat_buf;
	struct stat stat_spare;
	/* 如果文件1比文件2新，或者文件1存在，文件2不存在 */
	if (strcmp(symbol, "-nt") == 0) {
		return (stat(test1, &stat_buf) == 0 &&
			stat(test2, &stat_spare) == 0 &&
			stat_buf.st_mtime <= stat_spare.st_mtime);
	}
	/* 如果文件1和文件2 引用到相同的设备和inode编号 */
	if (strcmp(symbol, "-ef") == 0) {
		return (stat(test1, &stat_buf) == 0 &&
			stat(test2, &stat_spare) == 0 &&
			stat_buf.st_dev == stat_spare.st_dev &&
			stat_buf.st_ino == stat_spare.st_ino);
	}
	/* 如果文件1比文件2旧，或者文件1不存在，文件2存在 */
	if (strcmp(symbol, "-ot") == 0) {
		return (stat(test1, &stat_buf) == 0 &&
			stat(test2, &stat_spare) == 0 &&
			stat_buf.st_mtime >= stat_spare.st_mtime);
	}
	/* 整数相等 */
	if (strcmp(symbol, "-eq") == 0) {
		return(atoi(test1) == atoi(test2));
	}
	/* 整数大于等于 */
	if (strcmp(symbol, "-ge") == 0) {
		return(atoi(test1) >= atoi(test2));
	}
	/* 整数大于 */
	if (strcmp(symbol, "-gt") == 0) {
		return(atoi(test1) > atoi(test2));
	}
	/* 整数小于等于 */
	if (strcmp(symbol, "-le") == 0) {
		return(atoi(test1) <= atoi(test2));
	}
	/* 整数小于 */
	if (strcmp(symbol, "-lt") == 0) {
		return(atoi(test1) < atoi(test2));
	}
	/* 整数不等于 */
	if (strcmp(symbol, "-ne") == 0) {
		return(atoi(test1) != atoi(test2));
	}
	/* 字符串相等 */
	if (strcmp(symbol, "=") == 0) {
		return(strcmp(test1, test2) == 0);
	}
	/* 字符串不相等 */
	if (strcmp(symbol, "!=") == 0) {
		return(strcmp(test1, test2) != 0);
	}
	return 0;
}

/* 测试逻辑表达式 */
int my_test(char** cmd) {
	int argc = length(cmd);
	char** argv = cmd;
	int i = 0;
	while (argv[i] != NULL) {
		/* 与操作，分段测试 */
		if (strcmp(argv[i], "-a") == 0) {
			argv[i] = NULL;
			int flag1 = my_test(argv);
			int flag2 = my_test(&argv[i + 1]);
			return (flag1 && flag2);
		}
		/* 或操作，分段测试 */
		if (strcmp(argv[i], "-o") == 0) {
			argv[i] = NULL;
			int flag1 = my_test(argv);
			int flag2 = my_test(&argv[i + 1]);
			return (flag1 || flag2);
		}
		i++;
	}
	/* 如果有两个参数，则为单项逻辑表达式 */
	if (argc == 2)
		return single_test(argv);
	/* 如果有三个参数，则为双项逻辑表达式 */
	else if (argc == 3)
		return double_test(argv);
	else
		printf("test: invalid number of arguments\n");
	return 0;
}

/* 打印test的结果，用来显示测试的函数 */
void my_atest(char** cmd) {
	printf("%d\n", my_test(cmd));
}

/* 处理重定向 */
void my_redirect(char** cmd) {
	int argc = length(cmd);
	char** argv = cmd;
	int sin = dup(STDIN_FILENO);
	int sout = dup(STDOUT_FILENO);
	/* 从左至右依次处理重定向符号 */
	if (argc > 2) {
		if (strcmp(argv[1], "<") == 0) {
			int input_fd;
			input_fd = open(argv[2], O_RDONLY);
			dup2(input_fd, STDIN_FILENO);
			close(input_fd);
			if (argc == 3)
				argv[1] = NULL;
			int k = 0;
			/* 向左边移动两位 */
			for (k = 0; k != argc - 3; k++)
				argv[k + 1] = argv[k + 3];
		}
		else if (strcmp(argv[argc - 2], ">") == 0) {
			int write_fd;
			write_fd = open(argv[argc - 1], O_CREAT | O_RDWR | O_TRUNC, 0770);
			dup2(write_fd, STDOUT_FILENO);
			close(write_fd);
			argv[argc - 2] = NULL;
		}

		else if (strcmp(argv[argc - 2], ">>") == 0)
		{
			int write_fd;
			write_fd = open(argv[argc - 1], O_CREAT | O_RDWR | O_APPEND, 0770);
			dup2(write_fd, STDOUT_FILENO);
			close(write_fd);
			argv[argc - 2] = NULL;
		}
	}
	execute(argv);
	/* 恢复设置 */
	dup2(sin, STDOUT_FILENO);
	dup2(sout, STDIN_FILENO);
}

/* 执行命令 */
void my_exec(char** cmd) {
	int argc = length(cmd);
	char** argv = cmd;
	/* 开辟子进程执行命令 */
	pid_t pid = fork();
	if (pid < 0) {
		perror("exec");
	}
	else if (pid == 0) {
		execute(&argv[1]);
		exit(1);
	}
	else {
		waitpid(pid, NULL, 0);
	}
}

/* 测试过程时间 */
void my_time(char** cmd) {
	struct tms buf;
	int argc = length(cmd);
	char **argv = cmd;
	float realtime, usertime, systime;
	if (argc != 1 && argc != 2) {
		printf("time: invalid number of arguments\n");
		return;
	}
	/* 测试过程花费时间，并输出 */
	if (times(&buf) != -1) {
		usertime = buf.tms_utime + buf.tms_cutime;
		systime = buf.tms_stime + buf.tms_cstime;
		realtime = usertime + systime;

		printf("real\t%lf\n", realtime);
		printf("user\t%lf\n", usertime);
		printf("syt\t%lf\n", systime);
	}
	else {
		perror("time");
	}
}

/* 清楚当前页 */
void my_clear() {
	printf("%s", "\033[1H\033[2J");
}

/* "dir"的辅助函数，打印某一个路径的目录 */
void display_dir(char* path) {
	DIR *dir;
	struct dirent *dp1 = malloc(sizeof(struct dirent));
	struct dirent *dp2 = malloc(sizeof(struct dirent));
	if ((dir = opendir(path)) == NULL) {
		printf("dir: no such directory\n");
		return;
	}
	printf("%s:\n", path);
	/* 依次打印目录中所有文件的属性 */
	while (1) {
		if ((readdir_r(dir, dp1, &dp2)) != 0) {
			perror("readdir");
			return;
		}
		if (dp2 == NULL)
			break;
		if (dp2->d_name[0] == '.')
			continue;
		printf("inode = %d\t", (int)dp2->d_ino);
		printf("reclen = %d\t", (int)dp2->d_reclen);
		printf("name = %s\n", dp2->d_name);
	}
	closedir(dir);
	free(dp1);
	free(dp2);
}

/* 显示目录信息 */
void my_dir(char** cmd) {
	int argc = length(cmd);
	char** argv = cmd;
	if (argc == 1) {
		display_dir(".");
	}
	else {
		int i = 0;
		/* 依次显示每个目录参数 */
		while (argc - i > 1) {
			display_dir(argv[i + 1]);
			if (argc - i > 2)
				printf("\n");
			i++;
		}
	}
}


/* 提供帮助手册 */
void my_help() {
	/* 获得当前运行路径 */
	char* path = get_path();
	char* t = strrchr(path, '/');
	char* dir = malloc_clear(t - path + 2);
	strncpy(dir, path, t - path + 1);
	char* cmd = malloc_clear(strlen("more") + strlen(dir) + strlen("readme") + 2);
	strcat(cmd, "more ");
	strcat(cmd, " ");
	strcat(cmd, dir);
	strcat(cmd, "readme");
	/* 获得readme文件的路径 */
	/* 用"more"命令过滤readme文件 */
	char** split_cmd = splitline(cmd);
	execute(split_cmd);
	free(path);
	free(dir);
	free(cmd);
	free(split_cmd);
}

/* 打印所有的环境变量 */
void my_environ() {
	printf("environ=%p\n", environ);
	char** env = environ;
	while (*env != NULL) {
		printf("%s\n", *env++);
	}
}

/* 处理管道函数 */
int my_pipe(char** cmd) {
	int argc = length(cmd);
	char** argv = cmd;
	int i = 0, j = 1, status;
	int fd[20][2];
	int index[20];
	for (i = 0; i != 20; i++)
		index[i] = 0;
	int child[20];
	int pipe_num = 0;
	/* 统计管道数目 */
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "|") == 0)
			pipe_num++;
	}
	if (pipe_num != 0) {
		index[0] = 0;
		/* 根据管道符号将命令分段 */
		for (i = 0; i < argc; i++) {
			if (strcmp(argv[i], "|") == 0) {
				index[j] = i + 1;
				argv[i] = NULL;
				j++;
			}
		}
		for (i = 0; i < pipe_num; i++) {
			if (pipe(fd[i]) == -1) {
				fprintf(stderr, "pipe: open pipe error\n");
				return 0;
			}
		}
		int pid;
		i = 0;
		/* 第一个进程 */
		if ((pid = fork()) == 0) {
			close(fd[i][0]);
			if (dup2(fd[i][1], 1) == -1) {
				fprintf(stderr, "pipe: redirect standard out error\n");
				return -1;
			}
			close(fd[i][1]);
			execute(&argv[index[i]]);
			exit(1);
		}
		else {
			waitpid(child[i], &status, 0);
			close(fd[i][1]);
		}
		i++;
		/* 从左到右依次执行，并通过管道传输数据 */
		while (i < pipe_num)
		{
			if ((child[i] = fork()) == 0) {
				if (fd[i][0] != STDIN_FILENO) {
					if (dup2(fd[i - 1][0], STDIN_FILENO) == -1) {
						fprintf(stderr, "pipe: redirect standard in error\n");
						return -1;
					}
					close(fd[i - 1][0]);
					if (dup2(fd[i][1], STDOUT_FILENO) == -1) {
						fprintf(stderr, "pipe: redirect standard out error\n");
						return -1;
					}
					close(fd[i][1]);
				}
				execute(&argv[index[i]]);
				exit(1);
			}
			else
			{
				waitpid(child[i], &status, 0);
				close(fd[i][1]);
				i++;
			}
		}
		/* 最后一个进程 */
		if ((child[i] = fork()) == 0) {
			close(fd[i - 1][1]);
			if (fd[i - 1][0] != STDIN_FILENO) {
				if (dup2(fd[i - 1][0], STDIN_FILENO) == -1) {
					fprintf(stderr, "pipe: redirect standard in error\n");
					return -1;
				}
				close(fd[i - 1][0]);
			}
			execute(&argv[index[i]]);
			exit(1);
		}
		else if (!is_back) {
			close(fd[i - 1][1]);
			waitpid(child[i], NULL, 0);
		}
	}
	else {
		return 0;
	}
	return 0;
}

/* 退出shell */
void my_quit() {
	exit(0);
}

/* 移动参数列表，向左边移动1位 */
void my_shift() {
	if (para_count <= 0)
		return;
	para_count -= 1;
	char digit[MAXLINE];
	char next_digit[MAXLINE];
	/* 更新"#"变量 */
	memset(digit, '\0', MAXLINE);
	memset(next_digit, '\0', MAXLINE);
	sprintf(digit, "%d", para_count);
	my_para("#", digit);
	int i, total = 0;
	/* 更新"1", "2", "3"……变量 */
	for (i = 1; i <= para_count; i++) {
		memset(digit, '\0', MAXLINE);
		sprintf(digit, "%d", i);
		memset(next_digit, '\0', MAXLINE);
		sprintf(next_digit, "%d", i + 1);
		char* next = malloc_clear(strlen(next_digit) + 2);
		strcat(next, "$");
		strcat(next, next_digit);
		my_convert(next);
		my_para(digit, next);
		total += strlen(next);
		free(next);
	}
	/* 删除原先的最后一个参数 */
	total += para_count - 1;
	memset(digit, '\0', MAXLINE);
	sprintf(digit, "%d", para_count + 1);
	my_unset(digit);
	char* all = malloc_clear(total + 1);
	/* 更新"*"变量 */
	for (i = 1; i <= para_count; i++) {
		memset(digit, '\0', MAXLINE);
		sprintf(digit, "%d", i);
		char* t = malloc_clear(strlen(digit) + 2);
		strcat(t, "$");
		strcat(t, digit);
		my_convert(t);
		strcat(all, t);
		free(t);
		if (i != para_count)
			strcat(all, " ");
	}
	my_para("*", all);
	free(all);
}

/* 清楚所有的脚本参数变量 */
void my_clear_paras() {
	my_unset("#");
	my_unset("*");
	my_unset("$");
	int i = 0;
	for (i = 0; i <= para_count; i++) {
		memset(buffer, '\0', MAXLINE);
		sprintf(buffer, "%d", i);
		my_unset(buffer);
	}
	para_count = 0;
}

/* 跳过之后的所有命令 */
void my_continue() {
	run_flag = 0;
}

/* 读取文件，一行一行读取 */
void my_readline(char** ret, char* file_name) {
	FILE* in;
	int i = 0;
	memset(buffer, '\0', MAXLINE);
	if ((in = fopen(file_name, "r")) != NULL) {
		/* 未到文件结尾 */
		while (fgets(buffer, sizeof(buffer), in)) {
			ret[i] = malloc_clear(sizeof(char) * MAXLINE);
			if (ret[i] == NULL) {
				printf("shell: fail to apply space\n");
				return;
			}
			strcpy(ret[i], buffer);
			ret[i][strlen(ret[i]) - 1] = '\0';
			i++;
		}
	}
	ret[i] = NULL;
	fclose(in);
}

/* 执行文件脚本 */
void my_shell(char** cmd) {
	int argc = length(cmd);
	char** argv = cmd;
	/* 如果缺少参数，不断提示用户输入文件参数 */
	while (argv[1] == NULL) {
		printf("$ ");
		char** read = read_command();
		if (read != NULL && strlen(read[0]) != 0) {
			int j = 0;
			while (read[j] != NULL) {
				argv[j + 1] = malloc_clear(strlen(read[j]) + 1);
				strcpy(argv[j + 1], read[j]);
				j++;
			}
			argv[j + 1] = NULL;
		}
		free(read);
	}
	/* 设置相应的文件参数 */
	argc = length(argv);
	char* shell_line[MAXLINE];
	char* file = argv[1];
	my_paras(argv);
	my_readline(shell_line, file);
	int i = 0;
	/* 一行一行一次执行命令 */
	while (shell_line[i] != NULL) {
		char** split_line = splitline(shell_line[i]);
		execute(split_line);
		if (!run_flag) {
			my_clear_paras();
			run_flag = 1;
			return;
		}
		free(split_line);
		free(shell_line[i]);
		i++;
	}
	/* 清楚脚本变量 */
	my_clear_paras();
}

/* 外部命令，调用execvp执行 */
void my_normal(char** cmd) {
	execvp(cmd[0], cmd);
}

/* 内部命令 */
void my_internal(char** cmd) {
	char* line = cmd[0];
	if (!strcmp(line, "time")) {
		my_time(cmd);
		return;
	}
	if (!strcmp(line, "cd")) {
		my_cd(cmd);
		return;
	}
	if (!strcmp(line, "dir")) {
		my_dir(cmd);
		return;
	}
	if (!strcmp(line, "pwd")) {
		my_pwd();
		return;
	}
	if (!strcmp(line, "echo")) {
		my_echo(cmd);
		return;
	}
	if (!strcmp(line, "environ")) {
		my_environ();
		return;
	}
	if (!strcmp(line, "clr")) {
		my_clear();
		return;
	}
	if (!strcmp(line, "exit")) {
		my_exit(cmd);
		return;
	}
	if (!strcmp(line, "bg")) {
		my_bg(cmd);
		return;
	}
	if (!strcmp(line, "fg")) {
		my_fg(cmd);
		return;
	}
	if (!strcmp(line, "jobs")) {
		my_job();
		return;
	}
	if (!strcmp(line, "kill")) {
		my_kill(cmd);
		return;
	}
	if (!strcmp(line, "exec")) {
		my_exec(cmd);
		return;
	}
	if (!strcmp(line, "myshell")) {
		my_shell(cmd);
		return;
	}
	if (!strcmp(line, "set")) {
		my_set(cmd);
		return;
	}
	if (!strcmp(line, "unset")) {
		my_unset(cmd[1]);
		return;
	}
	if (!strcmp(line, "umask")) {
		my_umask(cmd);
		return;
	}
	if (!strcmp(line, "quit")) {
		my_quit();
		return;
	}
	if (!strcmp(line, "help")) {
		my_help();
		return;
	}
	if (!strcmp(line, "more")) {
		my_more(cmd);
		return;
	}
	if (!strcmp(line, "test")) {
		my_test(&cmd[1]);
		return;
	}
	if (!strcmp(line, "atest")) {
		my_atest(&cmd[1]);
		return;
	}
	if (!strcmp(line, "shift")) {
		my_shift();
		return;
	}
	if (!strcmp(line, "continue")) {
		my_continue();
		return;
	}
	if (!strcmp(line, "mv")) {
		my_mv(cmd);
		return;
	}
	if (!strcmp(line, "mkdir")) {
		my_mkdir(cmd);
		return;
	}
	if (!strcmp(line, "rm")) {
		my_rm(cmd);
		return;
	}
	if (!strcmp(line, "rmdir")) {
		my_rmdir(cmd);
		return;
	}
	if (!strcmp(line, "date")) {
		my_date();
		return;
	}
	if (!strcmp(line, "history")) {
		my_history();
		return;
	}
	if (!strcmp(line, "cp")) {
		my_cp(cmd);
		return;
	}
	if (!strcmp(line, "head")) {
		my_head(cmd);
		return;
	}
	if (!strcmp(line, "tail")) {
		my_tail(cmd);
		return;
	}
	if (!strcmp(line, "touch")) {
		my_touch(cmd);
		return;
	}
}

/* 执行命令函数 */
void execute(char** cmd) {
	if (cmd == NULL)
		return;
	pid_t pid1, pid2;
	int argc = length(cmd);
	char** argv = cmd;
	/* 判断是否是后台执行 */
	if (strcmp(argv[argc - 1], "&") == 0)
		is_back = 1;
	else
		is_back = 0;
	/* 判断是否是变量赋值 */
	if (argc == 1 && strchr(argv[0], '=')) {
		my_variable(argv[0]);
		return;
	}
	/* 后台执行情况 */
	if (is_back) {
		pid1 = fork();
		if (pid1 < 0) {
			perror("&");
		}
		else if (pid1 == 0) {
			/* 设置"parent"变量 */
			char* parent = get_path();
			set_path("parent", parent);
			free(parent);
			argv[argc - 1] = NULL;
			execute(argv);
			exit(0);
		}
		else {
			/* 取消屏蔽集 */
			sigprocmask(SIG_UNBLOCK, &son_set, NULL);
			son_pid = pid1;
			job *p = (job*)malloc(sizeof(job));
			strcpy(p->state, "running");
			strcpy(p->cmd, argv[0]);
			p->pid = pid1;
			add_job(p);
			kill(pid1, SIGCONT);
		}
	}
	else {
		/* 执行内部命令 */
		if (is_internal_cmd(argv) && !is_pipe(argv) && !is_io_redirect(argv)) {
			my_internal(argv);
			return;
		}
		/* 执行含管道命令 */
		if (is_pipe(argv)) {
			my_pipe(argv);
			return;
		}
		/* 执行含重定向命令 */
		if (is_io_redirect(argv)) {
			my_redirect(argv);
			return;
		}
		/* 执行外部命令 */
		if (is_normal(argv)) {
			pid2 = fork();
			if (pid2 < 0) {
				perror("myshell");
			}
			else if (pid2 == 0) {
				char* parent = get_path();
				set_path("parent", parent);
				free(parent);
				my_normal(argv);
			}
			else {
				sigprocmask(SIG_UNBLOCK, &son_set, NULL);
				son_pid = pid2;
				waitpid(pid2, NULL, 0);
			}
			return;
		}
	}
}
