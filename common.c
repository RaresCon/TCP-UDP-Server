#include "common.h"

int send_all(int sockfd, void *buf, int len)
{
	int sent_len = 0;
    char *buffer = buf;

	while (len) {
		int tmp = send(sockfd, buffer + sent_len, len, 0);

		if (tmp == 0) {
			break;
		}

		sent_len += tmp;
		len -= tmp;
	}

    return sent_len;
}

int recv_all(int sockfd, void *buf, int len)
{
	int recv_len = 0;
    char *buffer = buf;

	while (len) {
		int tmp = recv(sockfd, buffer + recv_len, len, 0);

		if (tmp == 0) {
			break;
		}

		recv_len += tmp;
		len -= tmp;
	}

    return recv_len;
}

int check_valid_uns_number(char *num)
{
    char check_number[10];
    sprintf(check_number, "%hd", atoi(num));

    int digit_no = 0;
    for (int i = 0; i < strlen(num); i++) {
        if (num[i] >= 48 && num[i] <= 57) {
            digit_no += 1;
        }
    }

    if (strlen(num) != strlen(check_number) || !digit_no) {
        return -1;
    }

    return atoi(num);
}

int init_socket(int addr_fam, int sock_type, int flags)
{
    int tcp_sockfd;
    if ((tcp_sockfd = socket(addr_fam, sock_type, 0)) < 0) {
        int retries = STD_RETRIES;
        while ((tcp_sockfd = socket(addr_fam, sock_type, 0)) < 0 && retries) {
            retries--;
        }
        if (!retries) {
            perror("socket(SOCK_STREAM) failed ");
            return -1;
        }
    }

    return tcp_sockfd;
}

int init_epoll(int max_events)
{
    int epoll_fd = epoll_create(max_events);
	if (epoll_fd == -1) {
		int retries = STD_RETRIES;
        while ((epoll_fd = epoll_create(max_events)) == -1 && retries) {
            retries--;
        }
        if (!retries) {
			perror("epoll_create() failed. Aborting...\n");
            return -1;
        }
	}

    return epoll_fd;
}

int add_event(int epoll_fd, int ev_fd, struct epoll_event *new_ev)
{
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ev_fd, new_ev) < 0) {
		int retries = STD_RETRIES;
        while (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ev_fd, new_ev) < 0 && retries) {
            retries--;
        }
        if (!retries) {
			perror("epoll_ctl(EPOLL_CTL_ADD) failed. Aborting...\n");
            return -1;
        }
	}
    return 0;
}

int send_command(int sockfd, comm_type type, uint8_t option, uint16_t buf_len)
{
    struct command_hdr command;
    command.opcode = type;
    command.option = option;
    command.buf_len = buf_len;

    if (send_all(sockfd, &command, sizeof(struct command_hdr)) < sizeof(struct command_hdr)) {
        return -1;
    }
    return 0;
}
