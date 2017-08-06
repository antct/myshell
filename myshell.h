/*
 * myshell.h
 *
 *  Created on: 2017-7-17
 *      Author: c
 */

#ifndef MYSHELL_H_
#define MYSHELL_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h> 
#include <sys/times.h> 
#include <signal.h> 
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>  
#include <utime.h>
// #include "my_ls.h"
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>

#define MAXLINE 128
#define MAXSIZE 1024
sigset_t son_set;
struct sigaction pact;
void sig_handler(int);
char* get_display_path();
char** read_command();
void execute(char**);
void init();



#endif /* MYSHELL_H_ */
