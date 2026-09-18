#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int parse_pid(const char *text, pid_t *pid) {
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value <= 0 ||
        value > INT_MAX) {
        return -1;
    }
    *pid = (pid_t)value;
    return 0;
}

static int parse_signal(const char *text, int *signal_number) {
    struct signal_name {
        const char *name;
        int number;
    } names[] = {
        {"SIGHUP", SIGHUP},   {"SIGINT", SIGINT},   {"SIGQUIT", SIGQUIT},
        {"SIGTERM", SIGTERM}, {"SIGUSR1", SIGUSR1}, {"SIGUSR2", SIGUSR2},
    };

    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (strcmp(text, names[i].name) == 0) {
            *signal_number = names[i].number;
            return 0;
        }
    }

    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    /* O intervalo de sinais em tempo real varia entre sistemas. O kernel
       fará a validação final e kill() retornará EINVAL se o número não existir. */
    if (errno == 0 && end != text && *end == '\0' && value > 0 &&
        value <= INT_MAX) {
        *signal_number = (int)value;
        return 0;
    }
    return -1;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <PID> <SINAL>\n", argv[0]);
        fprintf(stderr, "Exemplo: %s 1234 SIGUSR1\n", argv[0]);
        return EXIT_FAILURE;
    }

    pid_t pid;
    int signal_number;
    if (parse_pid(argv[1], &pid) != 0) {
        fprintf(stderr, "Erro: PID inválido: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    if (parse_signal(argv[2], &signal_number) != 0) {
        fprintf(stderr,
                "Erro: sinal inválido. Use um número ou SIGHUP, SIGINT, "
                "SIGQUIT, SIGTERM, SIGUSR1 ou SIGUSR2.\n");
        return EXIT_FAILURE;
    }

    if (kill(pid, 0) == -1) {
        if (errno == ESRCH) {
            fprintf(stderr, "Erro: o processo %ld não existe.\n", (long)pid);
        } else if (errno == EPERM) {
            fprintf(stderr, "Erro: sem permissão para sinalizar %ld.\n",
                    (long)pid);
        } else {
            perror("Erro ao verificar o processo");
        }
        return EXIT_FAILURE;
    }

    if (kill(pid, signal_number) == -1) {
        perror("Erro ao enviar o sinal");
        return EXIT_FAILURE;
    }

    printf("Sinal %s (%d) enviado ao processo %ld.\n", argv[2], signal_number,
           (long)pid);
    return EXIT_SUCCESS;
}
