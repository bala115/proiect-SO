#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

volatile sig_atomic_t keep_running = 1;

// Handler pentru SIGINT (Ctrl+C sau kill -SIGINT)
void handle_sigint(int sig) {
    // Folosim write() deoarece printf nu este async-signal-safe
    const char *msg = "\n[Monitor] Received SIGINT. Closing...\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    keep_running = 0;
}

// Handler pentru SIGUSR1
void handle_sigusr1(int sig) {
    const char *msg = "[Monitor] Notification: A new report has been added to a district!\n";
    write(STDOUT_FILENO, msg, strlen(msg));
}

int main() {
    struct sigaction sa_int, sa_usr1;

    // Configurare sigaction pentru SIGINT
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("Eroare la sigaction pentru SIGINT");
        return 1;
    }

    // Configurare sigaction pentru SIGUSR1
    sa_usr1.sa_handler = handle_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1) {
        perror("Eroare la sigaction pentru SIGUSR1");
        return 1;
    }

    // Creare si scriere PID in .monitor_pid
    int fd = open(".monitor_pid", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Eroare la crearea .monitor_pid");
        return 1;
    }

    char pid_str[32];
    int len = snprintf(pid_str, sizeof(pid_str), "%d\n", getpid());
    write(fd, pid_str, len);
    close(fd);

    printf("[Monitor] A pornit cu PID-ul %d. In asteptarea semnalelor...\n", getpid());

    // Bucla de asteptare. pause() pune procesul in asteptare pana vine un semnal
    while (keep_running) {
        pause(); 
    }

    // Cleanup inainte de exit
    if (unlink(".monitor_pid") == 0) {
        printf("[Monitor] Fila .monitor_pid a fost stearsa cu succes.\n");
    } else {
        perror("[Monitor] Eroare la stergerea .monitor_pid");
    }

    return 0;
}
