[ -d Build ] || mkdir Build &&
gcc -g server.c -o Build/server &&
./Build/server "$@"
