// Writer script for AESD assignment 2
// Author: Eric Percin, 1/23/2025

#include <syslog.h> 	// for logging using the LOG_USER facility
#include <stdio.h>	// for printf
#include <fcntl.h>      // for opening file
#include <unistd.h>     // for writing file
#include <errno.h>      // for errno
#include <string.h>     // for strlen and strerror

int main(int argc, char *argv[]) {

	// Set up syslog to log under user-level facility with no special options
	openlog("writer", 0, LOG_USER);

	// Check for argument validity
	if (argc != 3) {
		printf("Usage: writer <writefile> <writestr>\n");
		syslog(LOG_ERR, "Writer expected 2 arguments and received %d", argc - 1);
		return 1;
	}
	
	char *writefile = argv[1];
	char *writestr = argv[2];
	

	// Open the file and handle any errors. Write only, create
	int opened_fd = open(writefile, O_WRONLY | O_CREAT | O_TRUNC, 0664);
	
	if (opened_fd < 0) {
    		syslog(LOG_ERR, "Writer failed to open file %s with error %s", writefile, strerror(errno));
    		return 1;
	}

	// Write the file and handle any errors
	ssize_t num_bytes = write(opened_fd, writestr, strlen(writestr));
	close(opened_fd);
	
	if (num_bytes < 0) {
		syslog(LOG_ERR, "Writer failed to write file %s with error %s", writefile, strerror(errno));	
		return 1;
	}
	
	// Writing complete! Successful exit
	syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
	closelog();

	return 0;
}

