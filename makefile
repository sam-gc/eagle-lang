CORE_SOURCES=$(wildcard src/core/*.c)
COMPILER_SOURCES=$(wildcard src/compiler/*.c)
GRAMMAR_SOURCES=$(wildcard src/grammar/*.c)

CORE_OBJ_FILES=$(addprefix obj/core/,$(notdir $(CORE_SOURCES:.c=.o)))
COMPILER_OBJ_FILES=$(addprefix obj/compiler/,$(notdir $(COMPILER_SOURCES:.c=.o)))
GRAMMAR_OBJ_FILES=$(addprefix obj/grammar/,$(notdir $(GRAMMAR_SOURCES:.c=.o)))

CFLAGS=-g -Isrc -std=c99 -O0 -Wall `llvm-config --cflags`
LDFLAGS=`llvm-config --ldflags --system-libs --libs core support analysis`

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

obj/compiler/%.o: src/compiler/%.c
	$(MKDIR) obj/compiler/
	$(CC) $(CFLAGS) -c -o $@ $<

obj/core/%.o: src/core/%.c
	$(MKDIR) obj/core/
	$(CC) $(CFLAGS) -c -o $@ $<

obj/grammar/%.o: src/grammar/%.c
	$(MKDIR) obj/grammar/
	$(CC) $(CFLAGS) -c -o $@ $<

