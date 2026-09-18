#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t count_usr1 = 0;
static volatile sig_atomic_t count_usr2 = 0;
static volatile sig_atomic_t got_term = 0;

static void handle_usr1(int signal_number) {
    (void)signal_number;
    ++count_usr1;
}

static void handle_usr2(int signal_number) {
    (void)signal_number;
    ++count_usr2;
}

static void handle_term(int signal_number) {
    (void)signal_number;
    got_term = 1;
    running = 0;
}

static int install_handler(int signal_number, void (*handler)(int)) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    return sigaction(signal_number, &action, NULL);
}

static void report_events(void) {
    while (count_usr1 > 0) {
        --count_usr1;
        puts("[SINAL] SIGUSR1 recebido: mensagem 1.");
    }
    while (count_usr2 > 0) {
        --count_usr2;
        puts("[SINAL] SIGUSR2 recebido: mensagem 2.");
    }
    if (got_term) {
        got_term = 0;
        puts("[SINAL] SIGTERM recebido: encerramento solicitado.");
    }
}

int main(int argc, char **argv) {
    if (argc != 2 ||
        (strcmp(argv[1], "busy") != 0 && strcmp(argv[1], "blocking") != 0)) {
        fprintf(stderr, "Uso: %s <busy|blocking>\n", argv[0]);
        return EXIT_FAILURE;
    }

    setvbuf(stdout, NULL, _IONBF, 0);

    sigset_t handled, previous_mask;
    sigemptyset(&handled);
    sigaddset(&handled, SIGUSR1);
    sigaddset(&handled, SIGUSR2);
    sigaddset(&handled, SIGTERM);

    /* Bloquear durante a configuração evita perder um sinal antes da espera. */
    if (sigprocmask(SIG_BLOCK, &handled, &previous_mask) == -1 ||
        install_handler(SIGUSR1, handle_usr1) == -1 ||
        install_handler(SIGUSR2, handle_usr2) == -1 ||
        install_handler(SIGTERM, handle_term) == -1) {
        perror("Erro ao configurar sinais");
        return EXIT_FAILURE;
    }

    printf("Receptor iniciado. PID: %ld\n", (long)getpid());
    printf("Modo: %s. Sinais: SIGUSR1, SIGUSR2 e SIGTERM.\n", argv[1]);

    if (strcmp(argv[1], "busy") == 0) {
        if (sigprocmask(SIG_SETMASK, &previous_mask, NULL) == -1) {
            perror("sigprocmask");
            return EXIT_FAILURE;
        }
        while (running) {
            report_events();
        }
        report_events();
    } else {
        /* sigsuspend troca a máscara atomicamente e elimina a corrida de pause(). */
        while (running) {
            sigsuspend(&previous_mask);
            report_events();
        }
        report_events();
        sigprocmask(SIG_SETMASK, &previous_mask, NULL);
    }

    puts("Receptor finalizado.");
    return EXIT_SUCCESS;
}

