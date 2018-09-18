#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "student_code.h"

/**
 * This is the simple example from the BeeJ guide.
 */
void run_student_tests_wrapper_stats_1(struct f1_stats *stats)
{
	memset(stats, 0, sizeof(*stats));
	struct addrinfo hints, *rep, *rp;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	int s = getaddrinfo("inginious.info.ucl.ac.be", "443", &hints, &rep);
	if (s != 0) {
		fprintf(stderr, "%s\n", gai_strerror(s));
		return;
	}
	stats->getaddrinfo_success = true;
	int sfd = -1;
	for (rp = rep; rp != NULL; rp = rp->ai_next) {
		stats->nb_addr_tried++;
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1) {
			perror("socket");
			continue;
		}
		stats->nb_connect++;
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;
		perror("connect");
		close(sfd);
		sfd = -1;
	}
	freeaddrinfo(rep);
	if (sfd != -1) {
		char coucou[] = "coucou";
		ssize_t nsent;
		do {
			nsent = send(sfd, coucou, sizeof(coucou), 0);
			stats->nb_send++;
			if (nsent != -1) {
				stats->nb_bytes += nsent;
			} else {
				perror("send");
			}
			if (nsent != sizeof(coucou)) {
				break;
			}
		} while (nsent != -1 && stats->nb_bytes < 42);
	}
}

/*
 * The following two codes are adapted from the Linux man pages
 * for getaddrinfo
 */
void run_student_tests_wrapper_stats_2_client(struct stats2_client *stats)
{
	errno = 0;
	memset(stats, 0, sizeof(*stats));
	struct addrinfo hints, *rep, *rp;
	int sfd = -1, s;
	ssize_t nread;
	char buf[500];
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = 0;
	hints.ai_flags = 0;
	s = getaddrinfo(NULL, "1618", &hints, &rep);
	stats->ngai = 1;
	if (s == -1) {
		fprintf(stderr, "%s\n", gai_strerror(s));
		return;
	}
	for (rp = rep; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		stats->nsocket++;
		if (sfd == -1) {
			perror("socket");
			continue;
		}
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			stats->nconnect++;
			break;
		}
		perror("connect");
		stats->nconnect++;
		close(sfd);
		sfd = -1;
	}
	if (rp == NULL || sfd == -1)
		return;
	freeaddrinfo(rep);
	char *words[3];
	words[0] = "coucou";
	words[1] = "salut";
	words[2] = "STOP";
	for (int i = 0; i < 3; i++) {
		// Send the three messages
		int len = strlen(words[i]) + 1;
		// Let's use a struct msghdr to see if it works
		struct msghdr msg;
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		struct iovec msg_iov[1];
		msg_iov[0].iov_base = words[i];
		msg_iov[0].iov_len = len;
		msg.msg_iov = msg_iov;
		msg.msg_iovlen = 1;
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		msg.msg_flags = 0;
		ssize_t nsent = sendmsg(sfd, &msg, 0);
		if (nsent == -1)
			perror("send");
		stats->nsend++;
		if (nsent != len) {
			fprintf(stderr, "ACouldn't write\n");
			continue;
		}
		// Let's read the data again
		memset(buf, 0, sizeof(buf));
		nread = recv(sfd, buf, 500, 0);
		stats->nrecv++;
		if (nread == -1) {
			perror("recv");
			continue;
		}
		if (strcmp(buf, words[i]) != 0) {
			fprintf(stderr, "AMessage don't match\n");
			continue;
		}
	}
}

void run_student_tests_wrapper_stats_2_server(struct stats2_server *stats)
{
	errno = 0;
	memset(stats, 0, sizeof(*stats));
	struct addrinfo hints, *rep, *rp;
	int sfd = -1, s;
	struct sockaddr_storage peer_addr;
	socklen_t peer_addrlen;
	ssize_t nread;
	char buf[500];
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_PASSIVE;
	s = getaddrinfo(NULL, "1618", &hints, &rep);
	stats->ngai = 1;
	if (s == -1) {
		fprintf(stderr, "%s\n", gai_strerror(s));
		return;
	}
	for (rp = rep; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		stats->nsocket++;
		if (sfd == -1) {
			perror("Bsocket");
			continue;
		}
		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
			stats->nbind++;
			break;
		}
		perror("Bbind");
		stats->nbind++;
		close(sfd);
		sfd = -1;
	}
	if (rp == NULL || sfd == -1)
		return;
	freeaddrinfo(rep);
	for (int failures = 0; failures < 10;) {
		memset(buf, 0, sizeof(buf));
		// Stops at the reception of "stop"
		peer_addrlen = sizeof(peer_addr);
		nread = recvfrom(sfd, buf, 500, 0, (struct sockaddr*) &peer_addr, &peer_addrlen);
		stats->nrecvfrom++;
		if (nread == -1)
			perror("recvfrom");
		if (nread == -1) {
			failures++;
			continue;
		}
		// Let's skip the printing here
		ssize_t nsent = sendto(sfd, buf, nread, 0, (struct sockaddr*) &peer_addr, peer_addrlen);
		stats->nsendto++;
		if (nsent != nread) {
			failures++;
		}
		if (nsent == -1) {
			perror("Bsent");
		}
		if (strcmp(buf, "STOP") == 0) {
			break;
		}
	}
}

