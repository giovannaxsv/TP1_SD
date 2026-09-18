#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

enum { MESSAGE_SIZE = 20 };

static int is_prime(uint64_t value) {
    if (value < 2) return 0;
    if (value % 2 == 0) return value == 2;
    for (uint64_t divisor = 3; divisor <= value / divisor; divisor += 2) {
        if (value % divisor == 0) return 0;
    }
    return 1;
}

static int write_all(int fd, const void *buffer, size_t size) {
    const char *cursor = buffer;
    while (size > 0) {
        ssize_t written = write(fd, cursor, size);
        if (written < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        cursor += written;
        size -= (size_t)written;
    }
    return 0;
}

static int read_all(int fd, void *buffer, size_t size) {
    char *cursor = buffer;
    size_t total = 0;
    while (total < size) {
        ssize_t received = read(fd, cursor + total, size - total);
        if (received == 0) return total == 0 ? 0 : -1;
        if (received < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += (size_t)received;
    }
    return 1;
}

static int send_number(int fd, uint64_t value) {
    char message[MESSAGE_SIZE];
    memset(message, 0, sizeof(message));
    int length = snprintf(message, sizeof(message), "%llu",
                          (unsigned long long)value);
    if (length < 0 || length >= (int)sizeof(message)) return -1;
    return write_all(fd, message, sizeof(message));
}

static int parse_count(const char *text, uint64_t *count) {
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0) return -1;
    *count = (uint64_t)value;
    return 0;
}

int main(int argc, char **argv) {
    uint64_t count;
    if (argc != 2 || parse_count(argv[1], &count) != 0) {
        fprintf(stderr, "Uso: %s <quantidade_positiva>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int channel[2];
    if (pipe(channel) == -1) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    pid_t child = fork();
    if (child == -1) {
        perror("fork");
        close(channel[0]);
        close(channel[1]);
        return EXIT_FAILURE;
    }

    if (child == 0) {
        close(channel[1]);
        for (;;) {
            char message[MESSAGE_SIZE];
            int status = read_all(channel[0], message, sizeof(message));
            if (status <= 0) {
                if (status < 0) fprintf(stderr, "Consumidor: mensagem incompleta.\n");
                close(channel[0]);
                _exit(status == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
            }
            message[MESSAGE_SIZE - 1] = '\0';
            char *end = NULL;
            errno = 0;
            unsigned long long number = strtoull(message, &end, 10);
            if (errno != 0 || end == message) {
                fprintf(stderr, "Consumidor: mensagem numérica inválida.\n");
                close(channel[0]);
                _exit(EXIT_FAILURE);
            }
            if (number == 0) break;
            printf("Consumidor: %llu %s primo.\n", number,
                   is_prime((uint64_t)number) ? "é" : "não é");
            fflush(stdout);
        }
        puts("Consumidor: recebeu 0 e terminou.");
        fflush(stdout);
        close(channel[0]);
        _exit(EXIT_SUCCESS);
    }

    close(channel[0]);
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();
    uint64_t number = 1;
    int producer_failed = 0;
    for (uint64_t i = 0; i < count; ++i) {
        unsigned int delta = 1U + rand_r(&seed) % 100U;
        if (UINT64_MAX - number < delta) {
            fprintf(stderr, "Produtor: estouro da representação numérica.\n");
            producer_failed = 1;
            break;
        }
        number += delta;
        if (send_number(channel[1], number) != 0) {
            perror("Produtor: write");
            producer_failed = 1;
            break;
        }
        printf("Produtor: enviou %llu (delta=%u).\n",
               (unsigned long long)number, delta);
    }
    if (!producer_failed && send_number(channel[1], 0) != 0) {
        perror("Produtor: envio do terminador");
        producer_failed = 1;
    }
    close(channel[1]);

    int child_status;
    if (waitpid(child, &child_status, 0) == -1) {
        perror("waitpid");
        return EXIT_FAILURE;
    }
    if (producer_failed || !WIFEXITED(child_status) ||
        WEXITSTATUS(child_status) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
