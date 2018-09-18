#include <stdbool.h>
struct f1_stats {
	bool getaddrinfo_success;
	int nb_addr_tried;
	int nb_connect;
	int nb_send;
	ssize_t nb_bytes;
};
struct stats2_server {
	int ngai;
	int nsocket;
	int nbind;
	int nrecvfrom;
	int nsendto;
};
struct stats2_client {
	int ngai;
	int nsocket;
	int nconnect;
	int nsend;
	int nrecv;
};

void run_student_tests_wrapper_stats_1(struct f1_stats *stats);

void run_student_tests_wrapper_stats_2_client(struct stats2_client *stats);

void run_student_tests_wrapper_stats_2_server(struct stats2_server *stats);

// void run_student_tests_wrapper_stats_3();

