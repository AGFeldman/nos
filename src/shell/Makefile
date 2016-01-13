CFLAGS = -Wall -g

OBJS = mysh.o

all: mysh

mysh: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -L/usr/local/lib -I/usr/local/include -lreadline

clean: 
	rm -f *.o *~ mysh

.PHONY: all clean

