#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "CTester/CTester.h"
#include "CTester/read_write.h"

#define BILLION (1000*1000*1000)

void __real_exit(int status); // Needed as otherwise we'll get a segfault

struct read_fd_table_t {
	size_t n;
	struct read_item *items;
};
struct read_item {
	int fd;
	const struct read_buffer_t *buf;
	unsigned int chunk_id;
	size_t bytes_read;
	struct timespec last_time;
	int64_t interval;
};

bool timespec_is_between(const struct timespec *a, const struct timespec *min, const struct timespec *max)
{
	int64_t mint = min->tv_sec;
	mint *= BILLION;
	mint += min->tv_nsec;
	int64_t at = a->tv_sec;
	at *= BILLION;
	at += a->tv_nsec;
	int64_t maxt = max->tv_sec;
	maxt *= BILLION;
	maxt += max->tv_nsec;
	return (mint <= at && at <= maxt);
}

extern struct read_fd_table_t read_fd_table;

struct read_item;

extern bool fd_is_read_buffered(int fd);

extern ssize_t read_handle_buffer(int fd, void *buf, size_t len, int flags);

void test_fragmented_recv_before()
{
	set_test_metadata("fragmented_recv_before", _("Tests the use of before-intervals"), 1);
	// TODO
}

void test_fragmented_recv_after()
{
	set_test_metadata("fragmented_recv_after", _("Tests the use of after-intervals"), 1);
	// TODO
}

void test_fragmented_recv_realtime()
{
	set_test_metadata("fragmented_recv_realtime", _("Tests the use of real-time-intervals"), 1);
	// TODO, big thing
}

void test_fragmented_recv()
{
	set_test_metadata("fragmented_recv", _("Tests the use of a fragmented recv"), 1);
	MONITOR_ALL_RECV(monitored, true);
	reinit_network_socket_stats();
	reinit_read_fd_table();
	CU_ASSERT_EQUAL(read_fd_table.n, 0); // "read_fd_table is not empty"
	CU_ASSERT_EQUAL(read_fd_table.items, NULL); // "read_fd_table is not empty"
	size_t tab1len = 1000;
	char *tab1 = malloc(tab1len);
	if (!tab1)
		CU_FAIL_FATAL("Couldn't allocate memory");
	for (int i = 0; i < 1000; i++) {
		tab1[i] = (char)i;
	}
	struct read_buffer_t rbuf1, rbuf2, rbuf3;
	rbuf1.mode = READ_WRITE_BEFORE_INTERVAL;
	rbuf1.nchunks = 4;
	rbuf1.chunks = malloc(rbuf1.nchunks * sizeof(struct read_bufchunk_t));
	if (!rbuf1.chunks) {
		free(tab1);
		CU_FAIL_FATAL("Couldn't allocate memory");
	}
	rbuf1.chunks[0] = (struct read_bufchunk_t) {
		.buf = tab1,
		.buflen = 200,
		.interval = 2000
	};
	rbuf1.chunks[1] = (struct read_bufchunk_t) {
		.buf = (tab1 + 200),
		.buflen = 200,
		.interval = 3000
	};
	rbuf1.chunks[2] = (struct read_bufchunk_t) {
		.buf = (tab1 + 200 + 200),
		.buflen = 350,
		.interval = 2000
	};
	rbuf1.chunks[3] = (struct read_bufchunk_t) {
		.buf = (tab1 + 200 + 200 + 350),
		.buflen = 250,
		.interval = 2500
	};
	rbuf2.mode = READ_WRITE_AFTER_INTERVAL;
	rbuf2.nchunks = 1;
	rbuf2.chunks = malloc(rbuf2.nchunks * sizeof(struct read_bufchunk_t));
	if (!rbuf2.chunks) {
		free(rbuf1.chunks);
		free(tab1);
		CU_FAIL_FATAL("Couldn't allocate memory");
	}
	rbuf2.chunks[0] = (struct read_bufchunk_t) {
		.buf = tab1,
		.buflen = tab1len,
		.interval = 0
	};
	rbuf3.mode = READ_WRITE_REAL_INTERVAL;
	rbuf3.nchunks = 2;
	rbuf3.chunks = malloc(rbuf3.nchunks * sizeof(struct read_bufchunk_t));
	if (!rbuf3.chunks) {
		free(rbuf2.chunks);
		free(rbuf1.chunks);
		free(tab1);
		CU_FAIL_FATAL("Couldn't allocate memory");
	}
	rbuf3.chunks[0] = (struct read_bufchunk_t) {
		.buf = tab1,
		.buflen = tab1len,
		.interval = 4242
	};
	rbuf3.chunks[1] = (struct read_bufchunk_t) {
		.buf = tab1,
		.buflen = 0,
		.interval = 0
	};
	int fd1 = socket(AF_INET, SOCK_STREAM, 0);
	if (fd1 == -1) {
		free(tab1);
		free(rbuf1.chunks);
		CU_FAIL_FATAL("Couldn't allocate memory");
	}
	CU_ASSERT_EQUAL(set_read_data(fd1, &rbuf1), 0);
	CU_ASSERT_EQUAL(read_fd_table.n, 1);
	CU_ASSERT_TRUE(fd_is_read_buffered(fd1));
	CU_ASSERT_EQUAL(set_read_data(fd1 + 1, NULL), 0);
	CU_ASSERT_EQUAL(read_fd_table.n, 1);
	CU_ASSERT_TRUE(fd_is_read_buffered(fd1));
	CU_ASSERT_FALSE(fd_is_read_buffered(fd1 + 1));
	CU_ASSERT_EQUAL(set_read_data(fd1, NULL), 1);
	CU_ASSERT_EQUAL(read_fd_table.n, 0);
	CU_ASSERT_FALSE(fd_is_read_buffered(fd1));
	CU_ASSERT_FALSE(fd_is_read_buffered(fd1 + 1));

	struct timespec beforets1, afterts1, beforets2, afterts2, beforets3, afterts3;
	CU_ASSERT_EQUAL_FATAL(clock_gettime(CLOCK_REALTIME, &beforets1), 0);
	CU_ASSERT_EQUAL_FATAL(set_read_data(fd1, &rbuf1), 0);
	CU_ASSERT_EQUAL_FATAL(clock_gettime(CLOCK_REALTIME, &afterts1), 0);
	CU_ASSERT_EQUAL(read_fd_table.n, 1);
	CU_ASSERT_TRUE(fd_is_read_buffered(fd1));

	CU_ASSERT_EQUAL(set_read_data(fd1 + 1, &rbuf1), 0);
	CU_ASSERT_TRUE(fd_is_read_buffered(fd1 + 1));
	CU_ASSERT_EQUAL(read_fd_table.n, 2);
	CU_ASSERT_EQUAL(read_fd_table.items[0].buf, &rbuf1);
	CU_ASSERT_EQUAL(read_fd_table.items[0].fd, fd1);
	CU_ASSERT_EQUAL(read_fd_table.items[1].buf, &rbuf1);
	CU_ASSERT_EQUAL(read_fd_table.items[1].fd, (fd1+1));

	CU_ASSERT_EQUAL_FATAL(clock_gettime(CLOCK_REALTIME, &beforets3), 0);
	CU_ASSERT_EQUAL(set_read_data(fd1 + 2, &rbuf3), 0);
	CU_ASSERT_EQUAL_FATAL(clock_gettime(CLOCK_REALTIME, &afterts3), 0);
	CU_ASSERT_EQUAL(read_fd_table.n, 3);
	CU_ASSERT_TRUE(fd_is_read_buffered(fd1 + 2));
	CU_ASSERT_EQUAL(read_fd_table.items[0].buf, &rbuf1);
	CU_ASSERT_EQUAL(read_fd_table.items[0].fd, fd1);
	CU_ASSERT_EQUAL(read_fd_table.items[1].buf, &rbuf1);
	CU_ASSERT_EQUAL(read_fd_table.items[1].fd, (fd1+1));
	CU_ASSERT_EQUAL(read_fd_table.items[2].buf, &rbuf3);
	CU_ASSERT_EQUAL(read_fd_table.items[2].fd, (fd1+2));

	CU_ASSERT_EQUAL(set_read_data(fd1 + 1, &rbuf2), 1);
	CU_ASSERT_EQUAL(read_fd_table.n, 3);
	CU_ASSERT_TRUE(fd_is_read_buffered(fd1 + 1));
	CU_ASSERT_EQUAL(set_read_data(fd1 + 1, NULL), 1);
	CU_ASSERT_EQUAL(read_fd_table.n, 2);
	CU_ASSERT_FALSE(fd_is_read_buffered(fd1 + 1));
	CU_ASSERT_EQUAL(set_read_data(fd1 + 1, NULL), 0);
	CU_ASSERT_EQUAL(read_fd_table.n, 2);
	CU_ASSERT_FALSE(fd_is_read_buffered(fd1 + 1));
	CU_ASSERT_EQUAL(clock_gettime(CLOCK_REALTIME, &beforets2), 0);
	CU_ASSERT_EQUAL(set_read_data(fd1 + 1, &rbuf2), 0);
	CU_ASSERT_EQUAL(clock_gettime(CLOCK_REALTIME, &afterts2), 0);
	CU_ASSERT_EQUAL(read_fd_table.n, 3);
	CU_ASSERT_TRUE(fd_is_read_buffered(fd1));
	CU_ASSERT_TRUE(fd_is_read_buffered(fd1 + 1));
	CU_ASSERT_TRUE(fd_is_read_buffered(fd1 + 2));

	CU_ASSERT_EQUAL(read_fd_table.items[0].fd, fd1);
	CU_ASSERT_EQUAL(read_fd_table.items[0].buf, &rbuf1);
	CU_ASSERT_EQUAL(read_fd_table.items[0].chunk_id, 0);
	CU_ASSERT_EQUAL(read_fd_table.items[0].bytes_read, 0);
	CU_ASSERT_EQUAL(read_fd_table.items[0].interval, 0);
	CU_ASSERT_TRUE(timespec_is_between(&(read_fd_table.items[0].last_time), &beforets1, &afterts1));

	CU_ASSERT_EQUAL(read_fd_table.items[2].fd, fd1 + 1);
	CU_ASSERT_EQUAL(read_fd_table.items[2].buf, &rbuf2);
	CU_ASSERT_EQUAL(read_fd_table.items[2].chunk_id, 0);
	CU_ASSERT_EQUAL(read_fd_table.items[2].bytes_read, 0);
	CU_ASSERT_EQUAL(read_fd_table.items[2].interval, 0);
	CU_ASSERT_TRUE(timespec_is_between(&(read_fd_table.items[2].last_time), &beforets2, &afterts2));

	CU_ASSERT_EQUAL(read_fd_table.items[1].fd, fd1 + 2);
	CU_ASSERT_EQUAL(read_fd_table.items[1].buf, &rbuf3);
	CU_ASSERT_EQUAL(read_fd_table.items[1].chunk_id, 0);
	CU_ASSERT_EQUAL(read_fd_table.items[1].bytes_read, 0);
	CU_ASSERT_EQUAL(read_fd_table.items[1].interval, rbuf3.chunks[0].interval);
	CU_ASSERT_TRUE(timespec_is_between(&(read_fd_table.items[1].last_time), &beforets3, &afterts3));
	CU_ASSERT_EQUAL(set_read_data(fd1 + 2, NULL), 1); // We don't need it
	CU_ASSERT_EQUAL(read_fd_table.n, 2);
	// Correctly set up of the tests: done
	// Now, all we have to do is recv the data :-)
	reinit_read_fd_table();
	CU_ASSERT_EQUAL(read_fd_table.items, NULL);
	CU_ASSERT_EQUAL(read_fd_table.n, 0);
	CU_ASSERT_FALSE(fd_is_read_buffered(fd1));
	CU_ASSERT_FALSE(fd_is_read_buffered(fd1 + 1));
	CU_ASSERT_FALSE(fd_is_read_buffered(fd1 + 2));
	free(tab1);
	free(rbuf1.chunks);
	free(rbuf2.chunks);
	free(rbuf3.chunks);
}

// Check failures:
// Recall we must use the failures struct, and assign its (call), (call)_ret and (call)_errno fields.
// And we have the fileds FAIL_ALWAYS, FAIL_NEVER, FAIL_FIRST, FAIL_SECOND, FAIL_THIRD and FAIL_TWICE

int main(int argc, char **argv)
{
	RUN(
			test_fragmented_recv,
			test_fragmented_recv_before,
			test_fragmented_recv_after,
			test_fragmented_recv_realtime
	   );
}

