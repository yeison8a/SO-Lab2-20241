#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 1024
#define MAX_ARGS 100
#define MAX_COMMANDS 100

char error_message[30] = "An error has occurred\n";
char **path_list = NULL;
int path_count = 0;

void print_error() {
    write(STDERR_FILENO, error_message, strlen(error_message));
}

void set_path(char **tokens, int count) {
    for (int i = 0; i < path_count; i++) {
        free(path_list[i]);
    }
    free(path_list);

    path_count = count;
    path_list = malloc(sizeof(char *) * count);
    for (int i = 0; i < count; i++) {
        path_list[i] = strdup(tokens[i]);
    }
}

void free_path() {
    for (int i = 0; i < path_count; i++) {
        free(path_list[i]);
    }
    free(path_list);
}

void parse_and_execute(char *line) {
    char *commands[MAX_COMMANDS];
    int cmd_count = 0;

    // Dividir en comandos separados por &
    char *cmd = strtok(line, "&");
    while (cmd != NULL) {
        while (*cmd == ' ' || *cmd == '\t') cmd++; // saltar espacios
        if (strlen(cmd) > 0) {
            commands[cmd_count++] = strdup(cmd);
        }
        cmd = strtok(NULL, "&");
    }

    pid_t pids[MAX_COMMANDS];
    int pid_count = 0;

    for (int i = 0; i < cmd_count; i++) {
        int redirect = 0;
        char *redirect_file = NULL;

        // Detectar redirección
        char *redir = strchr(commands[i], '>');
        if (redir != NULL) {
            *redir = '\0';         // Comando hasta aquí
            redir++;
            // Validar que haya algo antes del '>'
            if (strlen(commands[i]) == 0) {
                print_error();
                continue;
            }

            // Saltar espacios y obtener nombre archivo
            while (*redir == ' ' || *redir == '\t') redir++;
            redirect_file = strtok(redir, " \t\n");
            if (redirect_file == NULL || strtok(NULL, " \t\n") != NULL) {
                print_error();
                continue;
            }
            redirect = 1;
        }

        // Parsear argumentos
        char *token;
        char *args[MAX_ARGS];
        int argc = 0;
        token = strtok(commands[i], " \t\n");
        while (token != NULL && argc < MAX_ARGS - 1) {
            args[argc++] = token;
            token = strtok(NULL, " \t\n");
        }
        args[argc] = NULL;

        if (argc == 0) {
            continue;
        }

        // Comandos integrados
        if (strcmp(args[0], "exit") == 0) {
            if (argc != 1) {
                print_error();
            } else {
                exit(0);
            }
        } else if (strcmp(args[0], "cd") == 0) {
            if (argc != 2 || chdir(args[1]) != 0) {
                print_error();
            }
        } else if (strcmp(args[0], "path") == 0) {
            set_path(&args[1], argc - 1);
        } else {
            // Comando externo
            pid_t pid = fork();
            if (pid == 0) {
                // Hijo
                char fullpath[512];
                int found = 0;
                for (int j = 0; j < path_count; j++) {
                    snprintf(fullpath, sizeof(fullpath), "%s/%s", path_list[j], args[0]);
                    if (access(fullpath, X_OK) == 0) {
                        found = 1;
                        break;
                    }
                }

                if (!found) {
                    print_error();
                    exit(1);
                }

                if (redirect) {
                    int fd = open(redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if (fd < 0) {
                        print_error();
                        exit(1);
                    }
                    dup2(fd, STDOUT_FILENO);
                    dup2(fd, STDERR_FILENO);
                    close(fd);
                }

                execv(fullpath, args);
                print_error();
                exit(1);
            } else if (pid > 0) {
                pids[pid_count++] = pid;
            } else {
                print_error();
            }
        }

        free(commands[i]);  // liberar cada comando
    }

    // Esperar todos los procesos paralelos
    for (int i = 0; i < pid_count; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

int main(int argc, char *argv[]) {
    FILE *input = stdin;
    int batch_mode = 0;

    if (argc > 2) {
        print_error();
        exit(1);
    } else if (argc == 2) {
        input = fopen(argv[1], "r");
        if (input == NULL) {
            print_error();
            exit(1);
        }
        batch_mode = 1;
    }

    // path inicial
    path_list = malloc(sizeof(char *));
    path_list[0] = strdup("/bin");
    path_count = 1;

    char *line = NULL;
    size_t len = 0;

    while (1) {
        if (!batch_mode) {
            printf("wish> ");
        }

        ssize_t read = getline(&line, &len, input);
        if (read == -1) {
            break;
        }

        parse_and_execute(line);
    }

    free(line);
    free_path();
    return 0;
}
