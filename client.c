#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/**
 * DEFINE CONSTANTS
 */
char* VALID_ARGS[] = {"-i", "-p", "-h", NULL};
int STRLEN = 1024;
int DEBUG = 1;
char* DEFAULT_IP = "127.0.0.1";

void failwith(char* message) {
    fprintf(stderr, "Error: %s\n", message);
    exit(1);
}

/**
 * @brief returns if argument is of valid arguments
 * returns 1 if so, 0 else
 * @param arg 
 * @return int 
 */
int valid_argument(char* arg) {
    int i = 0;
    while(VALID_ARGS[i] != NULL) {
        if(strcmp(VALID_ARGS[i], arg) == 0) {
            return 1;
        }
        i++;
    }

    return 0;
}

int main(int argc, char** argv) {
    char question_file[STRLEN];
    char ip[STRLEN];
    int port = 25555;
    int help = 0;

    // set up string argument defaults
    strcpy(ip, DEFAULT_IP);

    /**
     * parse process arguments
     */
    for(int i = 0; i < argc; i++) {
        char* arg = argv[i];
        char* argn = argv[i+1];

        // ip address argument
        if (strcmp(arg, "-i") == 0) {
            if(strlen(argn) >= STRLEN) {
                failwith("IP argument too long");
            }
            memset(ip, 0, sizeof(char)*STRLEN);
            strcpy(ip, argn);
        }

        // port number argument
        else if (strcmp(arg, "-p") == 0) {
            port = atoi(argn);
        }

        // help argument
        else if (strcmp(arg, "-h") == 0) {
            help = 1;
        }

        // invalid argument
        else if (arg[0] == '-') {
            fprintf(stderr, "Error: Unknown option '%s' recieved.\n", arg);
            exit(1);
        }
    }

    /**
     * DEBUG: Log arugments
     */
    if(DEBUG) {
        fprintf(stdout, "[DEBUG] ARGUMENTS:\n");
        fprintf(stdout, "|  ip: %s\n", ip);
        fprintf(stdout, "|  port: %d\n", port);
        fprintf(stdout, "|  help: %d\n", help);
    }

    return 0;
}