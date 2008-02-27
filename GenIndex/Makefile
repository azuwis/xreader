TARGET=GenIndex.exe
CC=gcc
CFLAGS=-g -O3 -Wall -DDEBUG -Wdeclaration-after-statement 
OBJ=main.o dbg.o
all: $(TARGET)
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^
.PHONY: clean
clean:
	rm -f $(OBJ) $(TARGET)
#dep
dbg.o: dbg.c dbg.h
main.o: main.c config.h dbg.h
