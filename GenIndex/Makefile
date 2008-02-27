TARGET=GenIndex.exe
CC=gcc
CFLAGS=-O3 -Wall -DDEBUG
OBJ=main.o dbg.o strsafe.o
all: $(TARGET)
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lws2_32
.PHONY: clean
clean:
	rm -f $(OBJ) $(TARGET)
#dep
dbg.o: dbg.c dbg.h strsafe.h
main.o: main.c config.h dbg.h
strsafe.o: strsafe.c strsafe.h
