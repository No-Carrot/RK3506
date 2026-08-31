#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void signal_handler(int signal_number)
{
	(void)signal_number;
	running = 0;
}

static int keep_cpu_dma_latency_open(void)
{
	int fd;
	int32_t latency = 0;

	while (running) {
		fd = open("/dev/cpu_dma_latency", O_WRONLY | O_CLOEXEC);
		if (fd < 0) {
			sleep(1);
			continue;
		}

		while (running) {
			if (write(fd, &latency, sizeof(latency)) ==
			    (ssize_t)sizeof(latency))
				return fd;
			if (errno == EINTR)
				continue;
			close(fd);
			sleep(1);
			break;
		}
	}

	errno = EINTR;
	return -1;
}

int main(void)
{
	struct sigaction sa;
	int fd;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);

	prctl(PR_SET_NAME, "cpu_dma_latency", 0, 0, 0);

	fd = keep_cpu_dma_latency_open();
	if (fd < 0)
		return 0;

	while (running)
		pause();

	close(fd);
	return 0;
}
