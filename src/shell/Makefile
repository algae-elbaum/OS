CFLAGS = -Wall -g

OBJS = mysh.o

all: mysh

mysh: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lreadline

clean: 
	rm -f *.o *~ mysh

.PHONY: all clean

