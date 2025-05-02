#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    char* question_file = "qshort.txt";
    char* ip = "127.0.0.1";
    int port = 25555;
    int help = 0;

    // parse process arguments
    for(int i = 0; i < argc; i++) {
        char* arg = argv[i];

        fprintf(stdout, "I'm the server!\n");

        // question file argument
        if(strcmp(arg, "-f") == 0) {
            
        }
    }
}