#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    double time_seconds;
    size_t occupancy;
    char operation;
} occupancy_event;

typedef struct {
    uint32_t *buffer;
    size_t capacity;
    size_t input_index;
    size_t output_index;
    size_t occupied;
    size_t producer_count;
    size_t consumer_count;
    size_t target;
    int quiet;
    const char *occupancy_path;
    sem_t empty_slots;
    sem_t full_slots;
    sem_t mutex;
    atomic_size_t next_ticket;
    atomic_size_t primes_found;
    atomic_int failed;
    occupancy_event *events;
    size_t event_count;
    struct timespec start;
} shared_state;

typedef struct {
    shared_state *shared;
    size_t id;
} thread_argument;

static double elapsed_seconds(const struct timespec *start) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)(now.tv_sec - start->tv_sec) +
           (double)(now.tv_nsec - start->tv_nsec) / 1000000000.0;
}

static int wait_semaphore(sem_t *semaphore) {
    while (sem_wait(semaphore) == -1) {
        if (errno != EINTR) return -1;
    }
    return 0;
}

static int is_prime(uint32_t value) {
    if (value < 2) return 0;
    if (value % 2 == 0) return value == 2;
    for (uint32_t divisor = 3; divisor <= value / divisor; divisor += 2) {
        if (value % divisor == 0) return 0;
    }
    return 1;
}

/* Chamada com o mutex do buffer adquirido. */
static void log_event(shared_state *shared, char operation) {
    if (shared->events == NULL || shared->event_count >= 2 * shared->target) {
        return;
    }
    occupancy_event *event = &shared->events[shared->event_count++];
    event->time_seconds = elapsed_seconds(&shared->start);
    event->occupancy = shared->occupied;
    event->operation = operation;
}

static void *producer(void *raw_argument) {
    thread_argument *argument = raw_argument;
    shared_state *shared = argument->shared;
    unsigned int seed = (unsigned int)time(NULL) ^
                        (unsigned int)(getpid() * 2654435761U) ^
                        (unsigned int)(argument->id * 2246822519U);

    for (;;) {
        size_t ticket = atomic_fetch_add(&shared->next_ticket, 1);
        if (ticket >= shared->target) break;
        uint32_t value = 1U + rand_r(&seed) % 10000000U;

        if (wait_semaphore(&shared->empty_slots) != 0 ||
            wait_semaphore(&shared->mutex) != 0) {
            atomic_store(&shared->failed, 1);
            return NULL;
        }
        shared->buffer[shared->input_index] = value;
        shared->input_index = (shared->input_index + 1) % shared->capacity;
        ++shared->occupied;
        log_event(shared, 'P');
        sem_post(&shared->mutex);
        sem_post(&shared->full_slots);
    }
    return NULL;
}

static void *consumer(void *raw_argument) {
    thread_argument *argument = raw_argument;
    shared_state *shared = argument->shared;

    for (;;) {
        if (wait_semaphore(&shared->full_slots) != 0 ||
            wait_semaphore(&shared->mutex) != 0) {
            atomic_store(&shared->failed, 1);
            return NULL;
        }
        uint32_t value = shared->buffer[shared->output_index];
        shared->output_index = (shared->output_index + 1) % shared->capacity;
        --shared->occupied;
        if (value != 0) log_event(shared, 'C');
        sem_post(&shared->mutex);
        sem_post(&shared->empty_slots);

        if (value == 0) break;
        int prime = is_prime(value);
        if (prime) atomic_fetch_add(&shared->primes_found, 1);
        if (!shared->quiet) {
            printf("Consumidora %zu: %u %s primo.\n", argument->id, value,
                   prime ? "é" : "não é");
        }
    }
    return NULL;
}

static int parse_size(const char *text, size_t *result, int allow_zero) {
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        (!allow_zero && value == 0) || value > SIZE_MAX) {
        return -1;
    }
    *result = (size_t)value;
    return 0;
}

static int save_occupancy(const shared_state *shared) {
    if (shared->occupancy_path == NULL) return 0;
    FILE *file = fopen(shared->occupancy_path, "w");
    if (file == NULL) {
        perror("Erro ao criar CSV de ocupação");
        return -1;
    }
    fprintf(file, "operation,time_seconds,occupancy\n");
    for (size_t i = 0; i < shared->event_count; ++i) {
        fprintf(file, "%zu,%.9f,%zu\n", i + 1, shared->events[i].time_seconds,
                shared->events[i].occupancy);
    }
    if (fclose(file) != 0) {
        perror("Erro ao fechar CSV de ocupação");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr,
                "Uso: %s N Np Nc [M] [--quiet] [--occupancy arquivo.csv]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    shared_state shared;
    memset(&shared, 0, sizeof(shared));
    shared.target = 100000;
    if (parse_size(argv[1], &shared.capacity, 0) != 0 ||
        parse_size(argv[2], &shared.producer_count, 0) != 0 ||
        parse_size(argv[3], &shared.consumer_count, 0) != 0) {
        fprintf(stderr, "Erro: N, Np e Nc devem ser inteiros positivos.\n");
        return EXIT_FAILURE;
    }

    int argument_index = 4;
    if (argument_index < argc && argv[argument_index][0] != '-') {
        if (parse_size(argv[argument_index], &shared.target, 0) != 0) {
            fprintf(stderr, "Erro: M deve ser um inteiro positivo.\n");
            return EXIT_FAILURE;
        }
        ++argument_index;
    }
    while (argument_index < argc) {
        if (strcmp(argv[argument_index], "--quiet") == 0) {
            shared.quiet = 1;
            ++argument_index;
        } else if (strcmp(argv[argument_index], "--occupancy") == 0 &&
                   argument_index + 1 < argc) {
            shared.occupancy_path = argv[argument_index + 1];
            argument_index += 2;
        } else {
            fprintf(stderr, "Erro: opção inválida ou incompleta: %s\n",
                    argv[argument_index]);
            return EXIT_FAILURE;
        }
    }

    if (shared.capacity > UINT_MAX) {
        fprintf(stderr, "Erro: N excede o limite do semáforo POSIX.\n");
        return EXIT_FAILURE;
    }
    if (shared.target > SIZE_MAX / 2 / sizeof(*shared.events)) {
        fprintf(stderr, "Erro: M grande demais.\n");
        return EXIT_FAILURE;
    }

    shared.buffer = calloc(shared.capacity, sizeof(*shared.buffer));
    if (shared.occupancy_path != NULL) {
        shared.events = calloc(2 * shared.target, sizeof(*shared.events));
    }
    pthread_t *producers = calloc(shared.producer_count, sizeof(*producers));
    pthread_t *consumers = calloc(shared.consumer_count, sizeof(*consumers));
    thread_argument *producer_arguments =
        calloc(shared.producer_count, sizeof(*producer_arguments));
    thread_argument *consumer_arguments =
        calloc(shared.consumer_count, sizeof(*consumer_arguments));
    if (shared.buffer == NULL || producers == NULL || consumers == NULL ||
        producer_arguments == NULL || consumer_arguments == NULL ||
        (shared.occupancy_path != NULL && shared.events == NULL)) {
        fprintf(stderr, "Erro: memória insuficiente.\n");
        free(shared.buffer); free(shared.events); free(producers); free(consumers);
        free(producer_arguments); free(consumer_arguments);
        return EXIT_FAILURE;
    }

    if (sem_init(&shared.empty_slots, 0, (unsigned int)shared.capacity) == -1 ||
        sem_init(&shared.full_slots, 0, 0) == -1 ||
        sem_init(&shared.mutex, 0, 1) == -1) {
        perror("sem_init");
        return EXIT_FAILURE;
    }
    atomic_init(&shared.next_ticket, 0);
    atomic_init(&shared.primes_found, 0);
    atomic_init(&shared.failed, 0);
    clock_gettime(CLOCK_MONOTONIC, &shared.start);

    size_t consumers_created = 0;
    size_t producers_created = 0;
    for (; consumers_created < shared.consumer_count; ++consumers_created) {
        consumer_arguments[consumers_created] =
            (thread_argument){&shared, consumers_created + 1};
        if (pthread_create(&consumers[consumers_created], NULL, consumer,
                           &consumer_arguments[consumers_created]) != 0) {
            fprintf(stderr, "Erro ao criar consumidora.\n");
            atomic_store(&shared.failed, 1);
            break;
        }
    }
    if (!atomic_load(&shared.failed)) {
        for (; producers_created < shared.producer_count; ++producers_created) {
            producer_arguments[producers_created] =
                (thread_argument){&shared, producers_created + 1};
            if (pthread_create(&producers[producers_created], NULL, producer,
                               &producer_arguments[producers_created]) != 0) {
                fprintf(stderr, "Erro ao criar produtora.\n");
                atomic_store(&shared.failed, 1);
                break;
            }
        }
    }

    for (size_t i = 0; i < producers_created; ++i) pthread_join(producers[i], NULL);

    /* Um zero por consumidora: marcadores de término inseridos no FIFO. */
    for (size_t i = 0; i < consumers_created; ++i) {
        wait_semaphore(&shared.empty_slots);
        wait_semaphore(&shared.mutex);
        shared.buffer[shared.input_index] = 0;
        shared.input_index = (shared.input_index + 1) % shared.capacity;
        ++shared.occupied;
        sem_post(&shared.mutex);
        sem_post(&shared.full_slots);
    }
    for (size_t i = 0; i < consumers_created; ++i) pthread_join(consumers[i], NULL);

    double total_seconds = elapsed_seconds(&shared.start);
    int save_failed = save_occupancy(&shared) != 0;
    printf("RESULT N=%zu Np=%zu Nc=%zu M=%zu seconds=%.9f primes=%zu events=%zu\n",
           shared.capacity, shared.producer_count, shared.consumer_count,
           shared.target, total_seconds, atomic_load(&shared.primes_found),
           shared.event_count);

    sem_destroy(&shared.empty_slots);
    sem_destroy(&shared.full_slots);
    sem_destroy(&shared.mutex);
    free(shared.buffer); free(shared.events); free(producers); free(consumers);
    free(producer_arguments); free(consumer_arguments);
    return atomic_load(&shared.failed) || save_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
