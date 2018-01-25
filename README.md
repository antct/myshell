# Myshell, version 1.0.0

## How to use?
1. first enter your shell directory, confrim files are complete(shell.h, shell.c, utility.c, makefile and readme). 
2. Then use your Linux terminal, enter "make myshell", you can use "./myshell" to run this shell if successful.
3. Of course, you can enter "make clean" to remove all object files.

## Represention rules
1. <parameter>: only one parameter.
2. [optional parameter]: parameter which may be omitted.
3. <parameters> ...: several parameters.
4. (expression): expression.
5. <type1|type2>: parameter can be type1 or type2. 

## About internal command
1. cd <directory> ...: change current working directory to the first directory if exists.
2. dir <directory> ...: output information of specified directories.
3. pwd: output information of current working directory.
4. echo <string|variable> ...: output an ordinary string or a variable.
5. clr: clear current displaying page.
6. exit [status]: exit will status if exists, otherwise, exit with status 0.
7. quit: exit with status 0.
8. more <file> ...: output contents full of displaying window size, use "q", "enter", "blank" to ctrl.
9. help: provide help manuals processed by instruction "more".
10. date: display current date and time.
11. time <process>: calculate time costing in the process.
12. exec <command> ...: execute commands.
13. set: output all variables with format "a=b".
14. unset <variable>: delete specified variable.
15. environ: output all environment variables with format "a=b".
16. bg <job>: move job to background.
17. fg <job>: move job on the stage.
18. jobs: output all jobs in background.
19. kill <job>: kill specified job.
20. umask [n]: output mask value or set mask value.
21. test [(expression)] <symbol> (expression): test logical relation between expressions.
22. atest [(expression)] <symbol> (expression): output the value after testing.
23. shift: move the parameter list to the left for one bit.
24. continue: skip after commands.
25. mv <file> <file>: move file from one directory to another.
26. cp <file> <file>: copy file from one directory to another.
27. touch <file> ...: create new files if not exists.
28. mkdir <directory> ...: create new directories if not exists.
29. rm <file> ...: delete files if exists.
30. rmdir <directory> ...: delete directories if exists.
31. history: output all previous commands.
32. head [n] <file>: display first n lines in file, set n as default value 10 if it omitted.
33. tail [n] <file>: display last n lines in file, set n as default value 10 if it omitted.
34. myshell <file> [<parameter> ...]: run batch file.

## About external command
1. external command will be executed by your original shell. Such as instructions "ls", "cat" and so on.
2. emmmmm, here exists a problem not solved that some variables may be different.
3. external command using external variables while internal commands using internal variables.

## About signal
1. when using this shell, ctrl+c are shielded.
2. when a child process is called, ctrl+z can hang this process stopped.
3. Arrows are not supported now.

## About environment
1. initial directory is your home directory in extern system.
2. initial variables are empty.
3. initial environment variables are "HOME", "PWD" and "shell".

## About pipe and redirection
1. pipe supported, but no more than 10 pipes please.
2. redirection supported, includeing '>', '>>' and '<'.

## About background
1. symbol "&" supported, process will be moved to background.

## Some notes
1. please enter your command separated with a blank strictly.
2. instruction "umask" can only change the value of internal shell rather than extern shell.
3. and so on.
