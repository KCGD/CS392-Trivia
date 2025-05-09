#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

/**
 * DEFINE CONSTANTS
 */
char *VALID_ARGS[] = {"-i", "-p", "-h", NULL};
int STRLEN = 1024;
int DEBUG = 0;
char *DEFAULT_IP = "127.0.0.1";
char *SOCK_DELIM = "|";
char SOCK_END = '\\';

enum Event_Dict {
  NAME_QUERY,
  NAME_RETURN,
  GAME_START,
  QUESTION_SEND,
  QUESTION_RESPONSE,
  ANSWER_BROADCAST,
  FECKOFF
};

void failwith(char *message) {
  fprintf(stderr, "Error: %s\n", message);
  exit(1);
}

/**
 * @brief print help doc with name of executable inserted
 *
 * @param execname
 */
void print_help(char *execname) {
  printf("Usage: %s [-i IP_address] [-p port_number] [-h]\n", execname);
  printf("\n");
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
int valid_argument(char *arg) {
  int i = 0;
  while (VALID_ARGS[i] != NULL) {
    if (strcmp(VALID_ARGS[i], arg) == 0) {
      return 1;
    }
    i++;
  }

  return 0;
}

/**
        source: https://stackoverflow.com/a/1798833
 */
void set_raw(int enable) {
  static struct termios oldt, newt;
  if (enable) {
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  } else {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  }
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
  char *src_str_saveptr = src_str;
  char *found;

  int result_pos = 0;

  // string spot
  while ((found = strsep(&src_str, delim)) != NULL) {
    strcpy(dest[result_pos], found);
    result_pos++;
  }

  free(found);
  free(src_str_saveptr);
  return result_pos + 1;
}

void print_question(char *prompt, char *options[3], int question_number) {
  printf("Question %d: %s\n", question_number, prompt);
  printf("Press 1: %s\n", options[0]);
  printf("Press 2: %s\n", options[1]);
  printf("Press 3: %s\n", options[2]);
}

// ssize_t sread(int fd, char *buffer, int size, char delim) {
//   ssize_t n;
//   while (buffer[strlen(buffer) - 1] != delim) {
//     n += read(fd, buffer, size);
//   }
//   return n;
// }

ssize_t swrite(int fd, char *message) {
  size_t written = 0;
  size_t length = strlen(message);
  while (written < length) {
    ssize_t n = write(fd, message + written, length - written);
    if (n < 0) {
      perror("write");
      return -1;
    }
    written += n;
  }

  return written;
}

void parse_connect(int argc, char **argv, int *server_fd) {
  char ip[STRLEN];
  int port = 25555;

  // set up string argument defaults
  strcpy(ip, DEFAULT_IP);

  for (int i = 0; i < argc; i++) {
    char *arg = argv[i];
    char *argn = argv[i + 1];

    // ip address argument
    if (strcmp(arg, "-i") == 0) {
      if (strlen(argn) >= STRLEN) {
        failwith("IP argument too long");
      }
      memset(ip, 0, sizeof(char) * STRLEN);
      strcpy(ip, argn);
    }

    // port number argument
    else if (strcmp(arg, "-p") == 0) {
      port = atoi(argn);
    }
  }

  // create socket to server
  struct sockaddr_in sock_addr;
  memset(&sock_addr, 0, sizeof(sock_addr));
  socklen_t addr_size = sizeof(sock_addr);
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_addr.s_addr = inet_addr(ip);
  sock_addr.sin_port = htons(port);

  *server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == 0) {
    failwith("Failed to create socket.");
  }

  // attempt connecting to server
  if (connect(*server_fd, (struct sockaddr *)&sock_addr, addr_size) == -1) {
    fprintf(stderr, "Failed to connect to server: %s:%d\n", ip, port);
    exit(1);
  }
}

int main(int argc, char **argv) {
  char question_file[STRLEN];
  int help = 0;

  /**
   * parse process arguments
   */
  for (int i = 0; i < argc; i++) {
    char *arg = argv[i];
    char *argn = argv[i + 1];

    // help argument
    if (strcmp(arg, "-h") == 0) {
      help = 1;
    }

    if (strcmp(arg, "-p") == 0 || strcmp(arg, "-i") == 0) {
      // defer to parse_connect
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
  if (DEBUG) {
    // fprintf(stdout, "[DEBUG] ARGUMENTS:\n");
    // fprintf(stdout, "|  ip: %s\n", ip);
    // fprintf(stdout, "|  port: %d\n", port);
    // fprintf(stdout, "|  help: %d\n", help);
  }

  if (help) {
    print_help(argv[0]);
    exit(0);
  }

  // connect to server and return socket
  int sock_fd;
  parse_connect(argc, argv, &sock_fd);

  // socket I/O
  char buffer[1024];
  memset(buffer, 0, sizeof(buffer));
  while (1) {
    int n;
    while (buffer[strlen(buffer) - 1] != SOCK_END) {
      n += read(sock_fd, buffer, 1024);
    }
    if (DEBUG) {
      printf("[DEBUG]: recieve:: %s\n", buffer);
    }

    // trim delimiter from buffer
    buffer[strlen(buffer) - 1] = 0;

    // socket close
    if (n == 0) {
      printf("socket closed.\n");
      close(sock_fd);
      exit(0);
    }

    // parse message
    char args[128][1024];
    int arg_num = split_by_delim(args, buffer, SOCK_DELIM);

    // handle server communications
    switch (atoi(args[0])) {
    // name queried from server
    // scan name and send back with NAME_RETURN
    case NAME_QUERY: {
      char command_buffer[1024];
      char name[128];

      printf("Please type your name: ");
      scanf("%s", name);

      snprintf(command_buffer, 1024, "%d|%s\\", NAME_RETURN, name);
      write(sock_fd, command_buffer, 1024);
    } break;

    // question recieve case. print and wait for input
    case QUESTION_SEND: {
      int question_number = atoi(args[1]);
      char *prompt = args[2];
      char *options[3] = {args[3], args[4], args[5]};
      print_question(prompt, options, question_number + 1);

      // set up stdio/socket reader (multiplex)
      int answered = 0;
      set_raw(1); // set "realtime" mode
      fd_set readfds;
      int max_fd = (STDIN_FILENO > sock_fd) ? STDIN_FILENO : sock_fd;

      FD_ZERO(&readfds);
      FD_SET(STDIN_FILENO, &readfds);
      FD_SET(sock_fd, &readfds);

      // await input
      int selecting = select(max_fd + 1, &readfds, NULL, NULL, NULL);

      if (FD_ISSET(STDIN_FILENO, &readfds)) {
        // stdin triggered, send message to server
        char ans;
        while (!answered) {
          if (read(STDIN_FILENO, &ans, 1) > 0) {
            int answered = 1;
            set_raw(0);
            break;
          }
        }

        // send response to server
        char res_buffer[1024];
        snprintf(res_buffer, 1024, "%d|%c\\", QUESTION_RESPONSE, ans);
        swrite(sock_fd, res_buffer);

        if (DEBUG) {
          printf("[DEBUG]: Answer %s\n", res_buffer);
        }
      } else {
        // socket triggered. print answer
        set_raw(0);
      }
    } break;

    // answer broadcase - print
    case ANSWER_BROADCAST: {
      int answered = 1;
      printf("%s\n", args[1]);
    } break;

    // exit case
    case FECKOFF: {
      shutdown(sock_fd, SHUT_RDWR);
      close(sock_fd);
      exit(0);
    } break;
    }

    memset(buffer, 0, sizeof(buffer));
  }

  return 0;
}