#include <signal.h>
#include <stdio.h>
#include <unistd.h>

void signal_handler(int signum) {
    printf("Signal %d received\n", signum);
    // 重新使能信号，以便可以再次处理
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, signum);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

int main() {
    signal(SIGINT, signal_handler);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_BLOCK, &set,NULL);

    while (1) {
        printf("Waiting for signal...\n");
        sleep(1);
    }

    return 0;
}