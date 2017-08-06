CC = gcc
OPTION1 = -o
OPTION2 = -c
OBJECTS = utility.o myshell.o
SOURCES = utility.c myshell.c
HEADERS = myshell.h
CLEARN = rm

myshell: $(OBJECTS)
	$(CC) $(OPTION1) myshell $(OBJECTS)

utility.o: utility.c myshell.h
	$(CC) $(OPTION2)  utility.c

myshell.o: myshell.c myshell.h
	$(CC) $(OPTION2)  myshell.c

clean:
	$(CLEARN) $(OBJECTS)
