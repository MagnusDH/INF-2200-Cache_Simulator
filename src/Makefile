CC = gcc
CFLAGS = -m32 -Wall -Werror -Wno-unused -g -O2 -I.
OBJDIR = obj
PROGRAM = cachesim

OBJS = cpu.o memory.o
HEADERS = byutr.h memory.h

all: $(PROGRAM) $(HEADERS) Makefile

dirs:
	@mkdir -p $(OBJDIR)

$(PROGRAM): $(patsubst %, $(OBJDIR)/%, $(OBJS))
	$(CC) $(CFLAGS) $^ -o $@

$(OBJDIR)/%.o: %.c dirs
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *.o *~ $(PROGRAM) $(OBJDIR)
