/*
 * utility.c
 *
 *  Created on: 2017-7-17
 *      Author: c
 */

#include "myshell.h"

int main() {
	init();
	int shouldrun = 1;
	/* 主函数处理信号函数 */
	pact.sa_handler = sig_handler;
	/* 设置屏蔽集 */
	sigemptyset(&son_set);
	sigaddset(&son_set, SIGTSTP);
	sigaddset(&son_set, SIGINT);
	while (shouldrun) {
		/* 屏蔽信号如ctrl+z和ctrl+c */
		sigprocmask(SIG_BLOCK, &son_set, NULL);
		signal(SIGINT, sig_handler);
		sigaction(SIGTSTP, &pact, NULL);
		/* 得到显示在shell中的路径 */
		char* shell = get_display_path();
		fprintf(stdout, "%s", shell);
		fflush(stdout);
		free(shell);
		/* 读取命令 */
		char** args = read_command();
		/* 执行命令 */
		execute(args);
		free(args);
	}
	return 0;
}
