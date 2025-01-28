#include "systemcalls.h"
#include <stdlib.h>	// system
#include <unistd.h>	// fork
#include <sys/wait.h>	// waitpid
#include <fcntl.h> 	// open
/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    // Call system() with command set in cmd. Return true on success
    int rc = system(cmd);
    return rc == 0 ;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    // Fork-execv-wait format leveraged from LSP page 161
    int status;
    pid_t pid = fork();
    if (pid == -1)
    	return false; // fork failed
    else if (pid == 0) {
        execv(command[0], command);
        exit(-1); // exit if execv fails
    
    }

    va_end(args);

    if (waitpid(pid, &status, 0) == -1)
    	return false; // waitpid failed
    else if (WIFEXITED(status))
    	return WEXITSTATUS(status) == 0; // return true on success

    return false;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

    // Redirect standard out to file. Leveraged from https://stackoverflow.com/a/13784315/1446624
    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0)
    	return false; // open failed
    
    // Fork-execv-wait format leveraged from LSP page 161
    int status;
    pid_t pid = fork();
    switch (pid) {
        case -1: 
            return false; // fork failed
            
        case 0: // child
            if (dup2(fd, STDOUT_FILENO) < 0) { // duplicate fd
                close(fd);
                exit(-1); // redirection of standard out failed
            }
            close(fd);
            execv(command[0], command);
            exit(-1); // execv failed
            
        default: // parent
            close(fd);
    }

    va_end(args);

    if (waitpid(pid, &status, 0) == -1)
    	return false; // waitpid failed
    else if (WIFEXITED(status))
    	return WEXITSTATUS(status) == 0; // return true on success

    return false;
}





















