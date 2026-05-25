CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
LIBS = -lcurl

SRCDIR = src
INCDIR = include
BINDIR = bin

SRCS = $(SRCDIR)/main.c $(SRCDIR)/cJSON.c
OBJS = $(BINDIR)/main.o $(BINDIR)/cJSON.o
TARGET = $(BINDIR)/cf-analyzer.exe

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	@if not exist $(BINDIR) mkdir $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LIBS)

$(BINDIR)/%.o: $(SRCDIR)/%.c
	@if not exist $(BINDIR) mkdir $(BINDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	del /Q $(OBJS) $(TARGET) 2>nul
	@if exist $(BINDIR) rmdir /S /Q $(BINDIR)

run: $(TARGET)
	./$(TARGET)