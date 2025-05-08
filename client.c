#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

/**
 * DEFINE CONSTANTS
 */
char* VALID_ARGS[] = {"-i", "-p", "-h", NULL};
int STRLEN = 1024;
int DEBUG = 1;
char* DEFAULT_IP = "127.0.0.1";
char* SOCK_DELIM = "|";

enum Event_Dict {
	NAME_QUERY,
	NAME_RETURN,
	GAME_START,

};

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

/**
 * @brief Split string by delim into string[]
 * 
 * @param dest 
 * @param str 
 * @param delim 
 * @return int 
 */
int split_by_delim(char dest[128][1024], char *str, char *delim) {
  char *src_str = strdup(str); // malloc()
  char *found;

  int result_pos = 0;

  // string spot
  while ((found = strsep(&src_str, delim)) != NULL) {
      strcpy(dest[result_pos], found);
      result_pos++;
  }

  free(found);
  free(src_str);
  return result_pos + 1;
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

    // create socket to server
    struct sockaddr_in sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));
    socklen_t addr_size = sizeof(sock_addr);
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = inet_addr(ip);
    sock_addr.sin_port = htons(port);

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd == 0) {
        failwith("Failed to create socket.");
    }

    // attempt connecting to server
    if(connect(sock_fd, (struct sockaddr*) &sock_addr, addr_size) == -1) {
        fprintf(stderr, "Failed to connect to server: %s:%d\n", ip, port);
        exit(1);
    }

    // socket I/O
    char buffer[1024];
		memset(buffer, 0, sizeof(buffer));
    while(1) {
        int n = read(sock_fd, buffer, 1024);
        buffer[n] = 0;

        // socket close
        if(n == 0) {
            printf("socket closed.\n");
            close(sock_fd);
            exit(0);
        }
        
        // parse message
        char args[128][1024];
        printf("%s\n", buffer);
        int arg_num = split_by_delim(args, buffer, SOCK_DELIM);
        for(int i=0; i < arg_num; i++) {
            printf("%d: %s\n", i, args[i]);
        }

				// handle server communications
				switch(atoi(args[0])) {
					// name queried from server
					// scan name and send back with NAME_RETURN
					case NAME_QUERY: {
						char command_buffer[1024];
						char name[128];

						printf("Please type your name: ");
						scanf("%s", name);

						snprintf(command_buffer, 1024, "%d|%s", NAME_RETURN, name);
						write(sock_fd, command_buffer, 1024);
					} break;
				}

        memset(buffer, 0, sizeof(buffer));
    }

    return 0;
}