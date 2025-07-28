CC=gcc
LIBS=-lws2_32
CFLAGS=-Wall -Wextra -g
LDFLAGS=$(LIBS)

CFILES=main.c libs/ipAddress/ipAddress.c libs/pargs/pargs.c libs/stringFunctions/stringFuncs.c
OFILES=$(CFILES:.c=.o)

TARGET=scanner

all: $(TARGET)

$(TARGET): $(OFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)  

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OFILES) $(TARGET)