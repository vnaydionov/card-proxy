
CFLAGS=-g -Wall -Wextra -pedantic-errors
OBJS=key_keeper.o util.o conf.o md5.o
TARGET=key_keeper

$(TARGET): $(OBJS)
	gcc $(CFLAGS) -o $@ $(OBJS)

.c.o:
	gcc $(CFLAGS) -c $<

key_keeper.o: key_keeper.c util.h conf.h

conf.o: conf.c conf.h util.h

util.o: util.c util.h md5.h

md5.o: md5.c md5.h

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

