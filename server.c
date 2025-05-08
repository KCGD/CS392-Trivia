#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/**
 * DEFINE CONSTANTS
 */
const int MAX_CLIENTS = 3;
char *QUESTION_DELIM = " ";
char *VALID_ARGS[] = {"-f", "-i", "-p", "-h", NULL};
int STRLEN = 1024;
int DEBUG = 1;
char *DEFAULT_QUESTION_FILE = "qshort.txt";
char *DEFAULT_IP = "127.0.0.1";

// define structs
struct Entry {
  char prompt[1024];
  char options[3][50];
  int answer_idx;
};

/**
 * @brief Print message to stderr and exit with error code 1
 *
 * @param message
 */
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
  printf("Usage: %s [-f question_file] [-i IP_address] [-p port_number] [-h]\n",
         execname);
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
 * @brief Split string into string[] by delimiter
 *
 * @param str
 * @param delim
 * @return int
 */
int split_by_delim(char dest[3][50], char *str, char *delim) {
  // char *src_str = strdup(str); // malloc()
  char *found;

  int result_pos = 0;

  // string spot
  while ((found = strsep(&str, delim)) != NULL) {
    if (result_pos < 3) {
      strcpy(dest[result_pos], found);
      result_pos++;
    }
  }

  free(found);
  return result_pos + 1;
}

void parse_error(char *filepath, int line_num, char *reason) {
  fprintf(stderr, "Parsing error: %s(%d) ~ %s\n", filepath, line_num, reason);
  exit(1);
}

/**
 * @brief read questions from question file
 * Will not be more than 50 questions, but check for that anyway
    destroy with destroy_entries
    derived from: https://stackoverflow.com/a/3501681/13307600
 * @param arr
 * @param filename
 * @return int number of questions read
 */
int read_questions(struct Entry *arr, char *filename) {
  FILE *fp;
  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  // keep track of position in entry array
  int entry_num = 0;
  int line_num = 1;

  // declare template struct
  struct Entry *this_entry = &arr[entry_num];

  /**
   * change behavior based on line number
   * 0 -> line separator
   * 1 -> prompt (string)
   * 2 -> questions (split_by_delim)
   * 3 -> answer (get index)
   * reset to 0 once question ended
   */
  int line_type = 1;

  // open question file
  if ((fp = fopen(filename, "r")) == NULL) {
    fprintf(stderr, "Failed to read question file: %s\n", filename);
    exit(1);
  }

  // read by line
  while ((read = getline(&line, &len, fp)) != -1) {
    // remove '\n' from line
    line[strlen(line) - 1] = '\0';

    // return (separator line)
    if (line_type == 0) {
      // separator type (0)
      if (strlen(line) > 0) {
        parse_error(filename, line_num, "Expected separator line.");
      }
      line_type++;
    }

    // prompt type (1)
    else if (line_type == 1) {
      if (strlen(line) < 2) {
        parse_error(filename, line_num,
                    "Expected prompt string (recieved empty line).");
      }
      strcpy(this_entry->prompt, line);
      line_type++;
    }

    // options type (2)
    else if (line_type == 2) {
      if (strlen(line) == 0) {
        parse_error(filename, line_num,
                    "Expected option string (recieved empty line).");
      }

      int length = split_by_delim(this_entry->options, line, QUESTION_DELIM);
      if (length != 4) {
        parse_error(filename, line_num, "Invalid amount of options.");
      }

      line_type++;
    }

    // answer index type (3)
    else if (line_type == 3) {
      int set = 0;
      for (int i = 0; i < 3; i++) {
        if (strcmp(line, this_entry->options[i]) == 0) {
          this_entry->answer_idx = i;
          set = 1;
        }
      }

      // error if supplied answer not in options
      if (set != 1) {
        parse_error(filename, line_num,
                    "Supplied answer not defined in options.");
      }

      line_type = 0;
      entry_num++;
      this_entry = &arr[entry_num];
    }

    line_num++;
  }

  free(line);
  fclose(fp);
  return entry_num;
}

void print_entry(struct Entry entry) {
  printf("Prompt: %s\n", entry.prompt);
  printf("Options: ");
  for (int i = 0; i < 3; i++) {
    printf("%s, ", entry.options[i]);
  }
  printf("\n");
  printf("Correct answer: %d (%s)\n", entry.answer_idx,
         entry.options[entry.answer_idx]);
}

int main(int argc, char **argv) {
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
  for (int i = 0; i < argc; i++) {
    char *arg = argv[i];
    char *argn = argv[i + 1];

    if (valid_argument(arg) && i + 1 >= argc && strcmp(arg, "-h") != 0) {
      failwith("Missing argument value");
    }

    // question file argument
    if (strcmp(arg, "-f") == 0) {
      if (strlen(argn) >= STRLEN) {
        failwith("file argument too long");
      }
      memset(question_file, 0, sizeof(char) * STRLEN);
      strcpy(question_file, argn);
    }

    // ip address argument
    else if (strcmp(arg, "-i") == 0) {
      if (strlen(argn) >= STRLEN) {
        failwith("IP argument too long");
      }
      memset(ip, 0, sizeof(char) * STRLEN);
      strcpy(ip, argn);
    }

    // port number argument
    else if (strcmp(arg, "-p") == 0) {
      port = atoi(argn);
      if (port == 0) {
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
  if (DEBUG) {
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
  if (sock_fd == 0) {
    failwith("Failed to create socket.");
  }

  // create socket address struct
  struct sockaddr_in incoming_sock_addr;
  memset(&incoming_sock_addr, 0, sizeof(incoming_sock_addr));
  struct sockaddr_in sock_addr;
  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_addr.s_addr = inet_addr(ip);
  sock_addr.sin_port = htons(port);

  // bind
  if (bind(sock_fd, (const struct sockaddr *)&sock_addr, sizeof(sock_addr)) <
      0) {
    failwith("Bind failed.");
  }

  // listen
  if (listen(sock_fd, MAX_CLIENTS) < 0) {
    failwith("Listen failed.");
  }

  // print welcome message (given socket suceeded)
  fprintf(stdout, "Welcome to 392 Trivia!\n");

  // test parsing questions
  struct Entry questions[50];
  int num_questions = read_questions(questions, question_file);

  // printf("Number of questions: %d\n", num_questions);
  // for (int i = 0; i < num_questions; i++) {
  //   print_entry(questions[i]);
  //   printf("\n");
  // }

  // start listening for players
  int clients[MAX_CLIENTS];
  socklen_t incoming_addr_size = sizeof(incoming_sock_addr);
  for (int i = 0; i < MAX_CLIENTS; i++) {
    clients[i] =
        accept(sock_fd, (struct sockaddr *)&sock_addr, &incoming_addr_size);
    if (clients[i] == -1) {
      failwith("Accept failed.");
    }
    printf("New connection detected!\n");
  }

  printf("Max connection reached!\n");

  return 0;
}