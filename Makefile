CFLAGS += -std=gnu99 -g -O2
CFLAGS += -Wall -Wextra -Wno-unused-parameter -Wno-strict-aliasing

include config.mak

LDFLAGS += 

all: framer-fuzz

OBJS= framer-fuzz.o

framer-fuzz: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

config.mak: configure
	./configure

clean:
	rm -f framer-fuzz framer-fuzz.o
