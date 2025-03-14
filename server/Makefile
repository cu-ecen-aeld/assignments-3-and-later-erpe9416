# MakeFile for AESD assignment 5
# Author: Eric Percin, 2/11/2025
# Reference: https://www.tutorialspoint.com/makefile/makefile_quick_guide.htm

# Take cross compiler as optional argument, or default to gcc:
CROSS_COMPILE ?=
CC ?= $(CROSS_COMPILE)gcc

TARGET ?= aesdsocket
SRCS = aesdsocket.c
OBJS = aesdsocket.o
CFLAGS ?= -Wall -Werror

USE_AESD_CHAR_DEVICE ?= 1
CFLAGS += -DUSE_AESD_CHAR_DEVICE=$(USE_AESD_CHAR_DEVICE)

all: $(TARGET)

default: all

# Linking
$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(TARGET)
	
# Compiling	
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
	
clean:
	rm -f $(TARGET) $(OBJS)
	
# Avoid confusing clean with a file name:
.PHONY: clean
