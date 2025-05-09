#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

/**
 * DEFINE CONSTANTS
 */
const int MAX_CLIENTS = 3;
char *QUESTION_DELIM = " ";
char *VALID_ARGS[] = {"-f", "-i", "-p", "-h", NULL};
int STRLEN = 1024;
int DEBUG = 0;
char *DEFAULT_QUESTION_FILE = "qshort.txt";
char *DEFAULT_IP = "127.0.0.1";
char *SOCK_DELIM = "|";
char SOCK_END = '\\';

// define structs
struct Entry {
  char prompt[1024];
  char options[3][50];
  int answer_idx;
};

struct GameState {
  int started;
  int clients_engaged;
  int ended;
  int question_number;
  int question_total;
  int question_pending;
  struct Entry active_question;
  struct Entry *question_set;
};

struct Player {
  int fd;
  int score;
  char name[128];
};

enum Event_Dict {
  NAME_QUERY,
  NAME_RETURN,
  GAME_START,
  QUESTION_SEND,
  QUESTION_RESPONSE,
  ANSWER_BROADCAST,
  FECKOFF
};
// define game state
struct GameState Game_State;

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
int split_option(char dest[3][50], char *str, char *delim) {
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
   * 2 -> questions (split_option)
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

      int length = split_option(this_entry->options, line, QUESTION_DELIM);
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

void print_question(char *prompt, char *options[3], int question_number) {
  printf("Question %d: %s\n", question_number, prompt);
  printf("Press 1: %s\n", options[0]);
  printf("Press 2: %s\n", options[1]);
  printf("Press 3: %s\n", options[2]);
}

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

/**
  Broadcase message to all clients
 */
void broadcast(struct Player clients[MAX_CLIENTS], char *message) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    swrite(clients[i].fd, message);
  }

  if (DEBUG) {
    printf("[DEBUG]: broadcast:: %s\n", message);
  }

  // prevent tcp message merging (happens sometimes if messages sent too
  // quickly)
  usleep(2 * 1000);
}

/**
  Handle game state and events
 */
void game_event(struct Player clients[MAX_CLIENTS]) {
  if (Game_State.started == 0) {
    // check if all players registered names
    int num_registered = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (DEBUG) {
        printf("[DEBUG]: client names: %s\n", clients[i].name);
      }

      if (strlen(clients[i].name) > 0) {
        num_registered++;
      }
    }

    // all clients registered, start game
    if (DEBUG) {
      printf("[DEBUG]: number registered: %d\n", num_registered);
    }
    if (num_registered == MAX_CLIENTS) {
      printf("The game starts now!\n");
      Game_State.started = 1;
      game_event(clients);
    }

    /**
    Game started
     */
  } else {
    // if all questions answered, print winner and exit
    if (Game_State.question_number == Game_State.question_total) {
      Game_State.ended = 1;
      int winner;
      int max_score = Game_State.question_total * -1; // lowest possible score
      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].score > max_score) {
          winner = i;
          max_score = clients[i].score;
        }
      }

      // print winner
      printf("Congrats, %s!\n", clients[winner].name);

      // tell everyone to leave
      char command_buffer[1024];
      snprintf(command_buffer, 1024, "%d\\", FECKOFF);
      broadcast(clients, command_buffer);

      // clean up and exit
      for (int i = 0; i < MAX_CLIENTS; i++) {
        shutdown(clients[i].fd, SHUT_RDWR);
        close(clients[i].fd);
      }

    }
    // if no pending question, ask
    else if (Game_State.question_pending == 0) {
      Game_State.active_question =
          Game_State.question_set[Game_State.question_number];

      // print active question to screen
      char *options[3] = {Game_State.active_question.options[0],
                          Game_State.active_question.options[1],
                          Game_State.active_question.options[2]};
      print_question(Game_State.active_question.prompt, options,
                     Game_State.question_number + 1);

      // broadcast question to all clients
      char command_buffer[1024];
      memset(command_buffer, 0, sizeof(command_buffer));
      snprintf(command_buffer, 1024, "%d|%d|%s|%s|%s|%s\\", QUESTION_SEND,
               Game_State.question_number, Game_State.active_question.prompt,
               Game_State.active_question.options[0],
               Game_State.active_question.options[1],
               Game_State.active_question.options[2]);

      // broadcast message to all clients
      broadcast(clients, command_buffer);
    } else {
      // take no action till question answered
    }
  }
}

/**
  handle client sockets and multiplexing
 */
void client_handler(struct Player clients[MAX_CLIENTS]) {
  // send client name query
  char command_buffer[1024];
  memset(command_buffer, 0, sizeof(command_buffer));
  snprintf(command_buffer, 1024, "%d\\", NAME_QUERY);
  broadcast(clients, command_buffer);
  Game_State.clients_engaged = 1;

  // set up read loop
  // use fd set write read fds for client returns
  char buffer[1024];
  memset(buffer, 0, sizeof(buffer));
  fd_set readfds;
  int max_fd = 0;

  while (1) {
    struct Player *active_client;

    // set up multiplex
    FD_ZERO(&readfds);

    for (int i = 0; i < MAX_CLIENTS; i++) {
      FD_SET(clients[i].fd, &readfds);
      if (clients[i].fd > max_fd) {
        max_fd = clients[i].fd;
      }
    }

    int selecting = select(max_fd + 1, &readfds, NULL, NULL, NULL);
    if (selecting < 0) {
      // safe case for when game ends
      // (client sockets close and select fails)
      if (Game_State.ended) {
        return;
      }
      failwith("Selecting failed.");
    }

    // find ready client
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (FD_ISSET(clients[i].fd, &readfds)) {
        active_client = &clients[i];
      }
    }

    // read from active client
    int n;
    while (buffer[strlen(buffer) - 1] != SOCK_END) {
      n += read(active_client->fd, buffer, 1024);
    }
    // trim delimiter from buffer
    buffer[strlen(buffer) - 1] = 0;

    // detect clietn losing connection
    if (n == 0) {
      printf("Lost connection!\n");
      close(active_client->fd);
    }

    // parse return
    char args[128][1024];
    int num_args = split_by_delim(args, buffer, SOCK_DELIM);

    switch (atoi(args[0])) {
    // name return
    case NAME_RETURN: {
      printf("Hi %s!\n", args[1]);
      strcpy(active_client->name, args[1]);
      game_event(clients);
    } break;

    // question response
    case QUESTION_RESPONSE: {
      if (DEBUG) {
        printf("[DEBUG]: Recieve answer: %s\n", args[1]);
      }
      // check if answer was correct
      if ((atoi(args[1]) - 1) == Game_State.active_question.answer_idx) {
        if (DEBUG) {
          printf("[DEBUG]: Answer correct! +1 ==> %s\n", active_client->name);
        }
        active_client->score++;
      } else {
        if (DEBUG) {
          printf("[DEBUG]: Answer incorrect. -1 ==> %s\n", active_client->name);
        }
        active_client->score--;
      }

      // broadcast correct answer
      char res_buffer[1024];
      snprintf(res_buffer, 1024, "%d|%s\\", ANSWER_BROADCAST,
               Game_State.active_question
                   .options[Game_State.active_question.answer_idx]);
      broadcast(clients, res_buffer);

      // queue next question
      Game_State.question_pending = 0;
      Game_State.question_number++;
      game_event(clients);
    } break;
    }
  }
}

int main(int argc, char **argv) {
  char question_file[STRLEN];
  char ip[STRLEN];
  int port = 25555;
  int help = 0;

  // set up game state
  memset(&Game_State, 0, sizeof(Game_State));
  Game_State.started = 0;
  Game_State.question_number = 0;
  Game_State.ended = 0;

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

  if (help) {
    print_help(argv[0]);
    exit(0);
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
  Game_State.question_total = num_questions;
  Game_State.question_set = questions;

  // start listening for players
  struct Player clients[MAX_CLIENTS];
  socklen_t incoming_addr_size = sizeof(incoming_sock_addr);
  for (int i = 0; i < MAX_CLIENTS; i++) {
    int client_fd =
        accept(sock_fd, (struct sockaddr *)&sock_addr, &incoming_addr_size);
    if (client_fd == -1) {
      failwith("Accept failed.");
    }

    // add client to clients array
    struct Player new_client;
    new_client.fd = client_fd;
    memset(new_client.name, 0, 128);
    clients[i] = new_client;

    printf("New connection detected!\n");
  }

  printf("Max connection reached!\n");
  client_handler(clients);

  // server cleanup
  for (int i = 0; i < 3; i++) {
    close(clients[i].fd);
  }
  close(sock_fd);

  return 0;
}