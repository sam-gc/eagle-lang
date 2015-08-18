CORE_SOURCES=$(wildcard src/core/*.c)
COMPILER_SOURCES=$(wildcard src/compiler/*.c)
GRAMMAR_SOURCES=$(wildcard src/grammar/*.c)
EXAMPLE_SOURCES=$(wildcard examples/*.egl)

CORE_OBJ_FILES=$(addprefix obj/core/,$(notdir $(CORE_SOURCES:.c=.o)))
COMPILER_OBJ_FILES=$(addprefix obj/compiler/,$(notdir $(COMPILER_SOURCES:.c=.o)))
GRAMMAR_OBJ_FILES=$(addprefix obj/grammar/,$(notdir $(GRAMMAR_SOURCES:.c=.o)))
EXAMPLE_EXECUTABLES=$(addprefix builtex/,$(notdir $(EXAMPLE_SOURCES:.egl=.e)))

CFLAGS=-g -Isrc -std=c99 -O0 -Wall `llvm-config --cflags`
LDFLAGS=`llvm-config --ldflags --system-libs --libs core support analysis native`

CC=gcc
MKDIR=mkdir -p
LD=g++

all: glory 

eagle: guts glory

guts: src/grammar/eagle.l src/grammar/eagle.y
	bison -d -o src/grammar/eagle.tab.c src/grammar/eagle.y -v
	lex -o src/grammar/tokens.c src/grammar/eagle.l

glory: $(COMPILER_OBJ_FILES) $(CORE_OBJ_FILES) $(GRAMMAR_OBJ_FILES)
	$(LD) -o eagle $^ $(LDFLAGS)

clean:
	rm -f eagle
	rm -rf obj
	rm -f src/grammar/eagle.tab.* src/grammar/tokens.c

deep_clean: clean
	rm -f *.s
	rm -f *.ll
	rm -rf builtex

clean-examples:
	rm -f *.e
	rm -rf builtex

obj/compiler/%.o: src/compiler/%.c
	$(MKDIR) obj/compiler/
	$(CC) $(CFLAGS) -c -o $@ $<

obj/core/%.o: src/core/%.c
	$(MKDIR) obj/core/
	$(CC) $(CFLAGS) -c -o $@ $<

obj/grammar/%.o: src/grammar/%.c
	$(MKDIR) obj/grammar/
	$(CC) $(CFLAGS) -c -o $@ $<

rc.o: rc.c
	$(CC) -c -g rc.c -o rc.o

prog: out.ll rc.o
	llc out.ll
	$(CC) out.s rc.o -g

%: examples/%.egl rc.o
	./eagle $< 2>out.ll
	llc out.ll
	$(CC) out.s rc.o -g -o $@.e

builtex/%.e: examples/%.egl
	$(MKDIR) builtex/
	./eagle $< 2> out.ll
	llc out.ll
	$(CC) out.s rc.o -g -o $@ -lm

all-examples: $(EXAMPLE_EXECUTABLES)


