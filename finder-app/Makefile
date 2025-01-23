# MakeFile for AESD assignment 2
# Author: Eric Percin, 1/23/2025
# Reference: https://www.tutorialspoint.com/makefile/makefile_quick_guide.htm

# Take cross compiler as optional argument, or default to gcc:
CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc

TARGET = writer
SRCS = writer.c
OBJS = writer.o

all: $(TARGET)

# Linking
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)
	
# Compiling	
%.o: %.c
	$(CC) -c $< -o $@
	
clean:
	rm -f $(TARGET) $(OBJS)
	
# Avoid confusing clean with a file name:
.PHONY: clean
