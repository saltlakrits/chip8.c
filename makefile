CC=clang
OUT = emu_sdl
CFLAGS=-Isrc/ -O0 -g -Wall -Wextra -fwrapv
SRCDIR = src/
#OBJDIR = .obj/
CFILES = $(wildcard $(SRCDIR)*.c)
OBJS = $(CFILES:.c=.o)

LDLIBS = -lSDL3
#@mkdir -p .obj


	#@./$(OUT)

main: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(OUT) $(LDLIBS)
	@printf "\n === Compiling program & deleting .o files ===\n"
	@rm -rf $(SRCDIR)*.o
