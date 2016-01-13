CFLAGS = -Wall -g
LDFLAGS =  -L/usr/local/lib -I/usr/local/include -lreadline

OBJS = mysh.o

all: mysh

mysh: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean: 
	rm -f *.o *~ mysh

.PHONY: all clean

