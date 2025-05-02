[ -d Build ] || mkdir Build &&
gcc -g client.c -o Build/client &&
./Build/client "$@"
