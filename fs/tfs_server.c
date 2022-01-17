#include "operations.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    unlink(pipename);

    if (mkfifo(pipename, 0777) < 0) {
        perror("Failed to create pipe");
        exit(EXIT_FAILURE);
    }

    int pipe_in = open(pipename, O_RDONLY);
    if (pipe_in < 0) {
        perror("Failed to open server pipe");
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_read;
    char op_code;
    while (1) {
        // main listener loop
        bytes_read = read(pipe_in, &op_code, sizeof(char));
        if (bytes_read <= 0)
            break;
        printf("Received op_code %d\n", op_code);
    }

    close(pipe_in);
    unlink(pipename);

    return 0;
}
