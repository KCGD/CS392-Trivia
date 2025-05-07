#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/**
 * DEFINE CONSTANTS
 */
const int MAX_CLIENTS = 3;
char* VALID_ARGS[] = {"-f", "-i", "-p", "-h", NULL};
int STRLEN = 1024;
int DEBUG = 1;
char* DEFAULT_QUESTION_FILE = "qshort.txt";
char* DEFAULT_IP = "127.0.0.1";

void failwith(char* message) {
    fprintf(stderr, "Error: %s\n", message);
    exit(1);
}

/**
 * @brief print help doc with name of executable inserted
 * 
 * @param execname 
 */
void print_help(char* execname) {
    printf("Usage: %s [-f question_file] [-i IP_address] [-p port_number] [-h]\n", execname);
    printf("\n");
    printf("  -f question_file    Default to \"qshort.txt\";\n");
    printf("  -i IP_address       Default to \"127.0.0.1\";\n");
    printf("  -p port_number      Default to 25555;\n");
    printf("  -h                  Display this help info.\n");
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
    strcpy(question_file, DEFAULT_QUESTION_FILE);
    strcpy(ip, DEFAULT_IP);

    /**
     * parse process arguments
     */
    for(int i = 0; i < argc; i++) {
        char* arg = argv[i];
        char* argn = argv[i+1];

        if(valid_argument(arg) && i+1 >= argc && strcmp(arg, "-h") != 0) {
            failwith("Missing argument value");
        }

        // question file argument
        if(strcmp(arg, "-f") == 0) {
            if(strlen(argn) >= STRLEN) {
                failwith("file argument too long");
            }
            memset(question_file, 0, sizeof(char)*STRLEN);
            strcpy(question_file, argn);
        }

        // ip address argument
        else if (strcmp(arg, "-i") == 0) {
            if(strlen(argn) >= STRLEN) {
                failwith("IP argument too long");
            }
            memset(ip, 0, sizeof(char)*STRLEN);
            strcpy(ip, argn);
        }

        // port number argument
        else if (strcmp(arg, "-p") == 0) {
            port = atoi(argn);
            if(port == 0) {
                failwith("Invalid port");
            }
        }

        // help argument
        else if (strcmp(arg, "-h") == 0) {
            help = 1;
            print_help(argv[0]);
            exit(0);
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
        fprintf(stdout, "|  quesiton_file: %s\n", question_file);
        fprintf(stdout, "|  ip: %s\n", ip);
        fprintf(stdout, "|  port: %d\n", port);
        fprintf(stdout, "|  help: %d\n", help);
    }

    /**
     * @brief set up server (listen on port)
     socket using domain -> AF_INET, type -> SOCK_STREAM, protocol -> 0?
     */
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd == 0) {
        failwith("Failed to create socket.");
    }

    // create socket address struct
    struct sockaddr_in sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = inet_addr(ip);
    sock_addr.sin_port = htons(port);

    // bind
    if(bind(sock_fd, (const struct sockaddr*)&sock_addr, sizeof(sock_addr)) < 0) {
        failwith("Bind failed.");
    }

    // listen
    if(listen(sock_fd, MAX_CLIENTS) < 0) {
        failwith("Listen failed.");
    }

    // print welcome message (given socket suceeded)
    fprintf(stdout, "Welcome to 392 Trivia!\n");



    /**
     * @brief server cleanup
     */
    close(sock_fd);

    return 0;
}