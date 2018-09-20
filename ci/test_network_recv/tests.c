#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include "CTester/CTester.h"
#include "CTester/read_write.h"

#define BILLION (1000*1000*1000)

#define TOL_INF_D 0.3
#define TOL_SUP_D 2.0
#define TOL_SUP_A (1000*1000)
#define TOL_INF_A 0

#define TIMESPEC_ZERO(ts) \
	struct timespec ts; \
	memset(&ts, 0, sizeof(ts));
#define TS_ZERO(ts) \
	ts.tv_sec = 0; ts.tv_nsec = 0;

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

extern int64_t get_time_interval(const struct timespec *pasttime, const struct timespec *curtime);

extern void getnanotime(struct timespec *res);

bool is_quasi_equal(int64_t a, int64_t b)
{
	double aa = a, bb = b;
	if (a != 0 && b != 0) {
		double cc = aa/bb;
		return (TOL_INF_D <= cc  && cc <= TOL_SUP_D);
	} else {
		if (a == 0) {
			return (TOL_INF_A <= b && b <= TOL_SUP_A);
		} else {
			return (TOL_INF_A <= a && a <= TOL_SUP_A);
		}
	}
}

extern bool wrap_monitoring;

void test_fragmented_recv()
{
	int64_t MILLION = 1000*1000;
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
	int fd1 = 17, fd2 = 42, fd3 = 0;
	CU_ASSERT_EQUAL(set_read_data(fd1, &rbuf1), 0);
	CU_ASSERT_EQUAL(read_fd_table.n, 1);
	CU_ASSERT_TRUE(fd_is_read_buffered(fd1));
	CU_ASSERT_EQUAL(set_read_data(fd2, NULL), 0);
	CU_ASSERT_EQUAL(read_fd_table.n, 1);
	CU_ASSERT_TRUE(fd_is_read_buffered(fd1));
	CU_ASSERT_FALSE(fd_is_read_buffered(fd2));
	CU_ASSERT_EQUAL(set_read_data(fd1, NULL), 1);
	CU_ASSERT_EQUAL(read_fd_table.n, 0);
	CU_ASSERT_FALSE(fd_is_read_buffered(fd1));
	CU_ASSERT_FALSE(fd_is_read_buffered(fd2));

	struct timespec beforets1, afterts1, beforets2, afterts2, beforets3, afterts3;
	CU_ASSERT_EQUAL_FATAL(clock_gettime(CLOCK_REALTIME, &beforets1), 0);
	CU_ASSERT_EQUAL_FATAL(set_read_data(fd1, &rbuf1), 0);
	CU_ASSERT_EQUAL_FATAL(clock_gettime(CLOCK_REALTIME, &afterts1), 0);
	CU_ASSERT_EQUAL(read_fd_table.n, 1);
	CU_ASSERT_TRUE(fd_is_read_buffered(fd1));

	CU_ASSERT_EQUAL(set_read_data(fd2, &rbuf1), 0);
	CU_ASSERT_TRUE(fd_is_read_buffered(fd2));
	CU_ASSERT_EQUAL(read_fd_table.n, 2);
	CU_ASSERT_EQUAL(read_fd_table.items[0].buf, &rbuf1);
	CU_ASSERT_EQUAL(read_fd_table.items[0].fd, fd1);
	CU_ASSERT_EQUAL(read_fd_table.items[1].buf, &rbuf1);
	CU_ASSERT_EQUAL(read_fd_table.items[1].fd, fd2);

	CU_ASSERT_EQUAL_FATAL(clock_gettime(CLOCK_REALTIME, &beforets3), 0);
	CU_ASSERT_EQUAL(set_read_data(fd3, &rbuf3), 0);
	CU_ASSERT_EQUAL_FATAL(clock_gettime(CLOCK_REALTIME, &afterts3), 0);
	CU_ASSERT_EQUAL(read_fd_table.n, 3);
	CU_ASSERT_TRUE(fd_is_read_buffered(fd3));
	CU_ASSERT_EQUAL(read_fd_table.items[0].buf, &rbuf1);
	CU_ASSERT_EQUAL(read_fd_table.items[0].fd, fd1);
	CU_ASSERT_EQUAL(read_fd_table.items[1].buf, &rbuf1);
	CU_ASSERT_EQUAL(read_fd_table.items[1].fd, fd2);
	CU_ASSERT_EQUAL(read_fd_table.items[2].buf, &rbuf3);
	CU_ASSERT_EQUAL(read_fd_table.items[2].fd, fd3);

	CU_ASSERT_EQUAL(set_read_data(fd2, &rbuf2), 1);
	CU_ASSERT_EQUAL(read_fd_table.n, 3);
	CU_ASSERT_TRUE(fd_is_read_buffered(fd2));
	CU_ASSERT_EQUAL(set_read_data(fd2, NULL), 1);
	CU_ASSERT_EQUAL(read_fd_table.n, 2);
	CU_ASSERT_FALSE(fd_is_read_buffered(fd2));
	CU_ASSERT_EQUAL(set_read_data(fd2, NULL), 0);
	CU_ASSERT_EQUAL(read_fd_table.n, 2);
	CU_ASSERT_FALSE(fd_is_read_buffered(fd2));
	CU_ASSERT_EQUAL(clock_gettime(CLOCK_REALTIME, &beforets2), 0);
	CU_ASSERT_EQUAL(set_read_data(fd2, &rbuf2), 0);
	CU_ASSERT_EQUAL(clock_gettime(CLOCK_REALTIME, &afterts2), 0);
	CU_ASSERT_EQUAL(read_fd_table.n, 3);
	CU_ASSERT_TRUE(fd_is_read_buffered(fd1));
	CU_ASSERT_TRUE(fd_is_read_buffered(fd2));
	CU_ASSERT_TRUE(fd_is_read_buffered(fd3));

	CU_ASSERT_EQUAL(read_fd_table.items[0].fd, fd1);
	CU_ASSERT_EQUAL(read_fd_table.items[0].buf, &rbuf1);
	CU_ASSERT_EQUAL(read_fd_table.items[0].chunk_id, 0);
	CU_ASSERT_EQUAL(read_fd_table.items[0].bytes_read, 0);
	CU_ASSERT_EQUAL(read_fd_table.items[0].interval, 0);
	CU_ASSERT_TRUE(timespec_is_between(&(read_fd_table.items[0].last_time), &beforets1, &afterts1));

	CU_ASSERT_EQUAL(read_fd_table.items[2].fd, fd2);
	CU_ASSERT_EQUAL(read_fd_table.items[2].buf, &rbuf2);
	CU_ASSERT_EQUAL(read_fd_table.items[2].chunk_id, 0);
	CU_ASSERT_EQUAL(read_fd_table.items[2].bytes_read, 0);
	CU_ASSERT_EQUAL(read_fd_table.items[2].interval, 0);
	CU_ASSERT_TRUE(timespec_is_between(&(read_fd_table.items[2].last_time), &beforets2, &afterts2));

	CU_ASSERT_EQUAL(read_fd_table.items[1].fd, fd3);
	CU_ASSERT_EQUAL(read_fd_table.items[1].buf, &rbuf3);
	CU_ASSERT_EQUAL(read_fd_table.items[1].chunk_id, 0);
	CU_ASSERT_EQUAL(read_fd_table.items[1].bytes_read, 0);
	fprintf(stderr, "%lld %lld\n", (long long)read_fd_table.items[1].interval, (long long)rbuf3.chunks[0].interval);
	CU_ASSERT_EQUAL(read_fd_table.items[1].interval, rbuf3.chunks[0].interval * MILLION);
	CU_ASSERT_TRUE(timespec_is_between(&(read_fd_table.items[1].last_time), &beforets3, &afterts3));
	CU_ASSERT_EQUAL(set_read_data(fd3, NULL), 1); // We don't need it
	CU_ASSERT_EQUAL(read_fd_table.n, 2);
	// Correctly set up of the tests: done
	// Now, all we have to do is recv the data :-)
	reinit_read_fd_table();
	CU_ASSERT_EQUAL(read_fd_table.items, NULL);
	CU_ASSERT_EQUAL(read_fd_table.n, 0);
	CU_ASSERT_FALSE(fd_is_read_buffered(fd1));
	CU_ASSERT_FALSE(fd_is_read_buffered(fd2));
	CU_ASSERT_FALSE(fd_is_read_buffered(fd3));
	free(tab1);
	free(rbuf1.chunks);
	free(rbuf2.chunks);
	free(rbuf3.chunks);
}

void test_fragmented_recv_before()
{
	//int64_t MILLION = 1000*1000;
	set_test_metadata("fragmented_recv_before", _("Tests the use of before-intervals"), 1);
	MONITOR_ALL_RECV(monitored, true);
	reinit_network_socket_stats();
	reinit_read_fd_table();
	int mode = READ_WRITE_BEFORE_INTERVAL;
	size_t tab1len = 1000 * sizeof(int);
	int *tab1 = malloc(tab1len);
	if (!tab1)
		CU_FAIL_FATAL("Memory");
	for (int i = 0; i < 1000; i++) {
		tab1[i] = i;
	}
	off_t offsets1[] = {200 * sizeof(int), 350 * sizeof(int), 445, 250 * sizeof(int), 200 * sizeof(int) - 445};
	int intervals1[] = {20, 25, 10, 15, 20};
	struct read_buffer_t rbuf1;
	rbuf1.mode = mode;
	if (create_partial_read_buffer(tab1, 5, offsets1, intervals1, &rbuf1)) {
		free(tab1);
		CU_FAIL_FATAL("Memory");
	}
	int fd1 = 42;
	int r = set_read_data(fd1, &rbuf1);
	if (r < 0) {
		free(tab1);
		free_partial_read_buffer(&rbuf1);
		CU_FAIL_FATAL("Memory");
	}
	CU_ASSERT_EQUAL(r, 0);
	TIMESPEC_ZERO(ts1);
	TIMESPEC_ZERO(ts2);
	TIMESPEC_ZERO(ts3);
	TIMESPEC_ZERO(ts4);
	TIMESPEC_ZERO(ts5);
	TIMESPEC_ZERO(ts6);
	TIMESPEC_ZERO(ts7);
	TIMESPEC_ZERO(ts8);
	TIMESPEC_ZERO(ts9);
	TIMESPEC_ZERO(ts10);
	ssize_t read1 = 0, read2 = 0, read3 = 0, read4 = 0, read5 = 0, read6 = 0, read7 = 0, read8 = 0, read9 = 0, cumread = 0;
	int buf1[1010];
	void *buf = buf1;
	SANDBOX_BEGIN;
	getnanotime(&ts1);
	read1 = recv(fd1, buf, 250 * sizeof(int), 0); // Should wait 20msec and return 200*4 bytes, next chunk
	cumread += read1;
	fprintf(stderr, "1 %d %p\n", (int)cumread, (buf + cumread));
	getnanotime(&ts2); // difference with 1 should be 20msec
	read2 = recv(fd1, (buf + cumread), 250 * sizeof(int), 0); // Should wait 25msec and return (50+200)*4 bytes
	cumread += read2;
	fprintf(stderr, "2 %d %p\n", (int)cumread, (buf + cumread));
	getnanotime(&ts3); // difference with 2 should be 25msec
	read3 = recv(fd1, (buf + cumread), 250 * sizeof(int), 0); // Shouldn't wait and return 100*4 bytes, next chunk
	cumread += read3;
	fprintf(stderr, "3 %d %p\n", (int)cumread, (buf + cumread));
	getnanotime(&ts4); // difference with 3 should be small
	read4 = recv(fd1, (buf + cumread), 250 * sizeof(int), 0); // Should wait about 10msec and return 445 bytes, next chunk
	cumread += read4;
	fprintf(stderr, "4 %d %p\n", (int)cumread, (buf + cumread));
	getnanotime(&ts5); // difference with 4 should be 10msec
	struct timespec tss1 = (struct timespec) {.tv_sec = 0, .tv_nsec = 20*1000*1000}; // Shouldn't have any impact
	nanosleep(&tss1, NULL);
	getnanotime(&ts6); // difference with 5 should be 20msec
	read5 = recv(fd1, (buf + cumread), 250 * sizeof(int), 0); // Should wait about 15msec and return 250*4 bytes, next chunk
	cumread += read5;
	fprintf(stderr, "5 %d %p\n", (int)cumread, (buf + cumread));
	getnanotime(&ts7); // difference with 6 should be 15msec
	read6 = recv(fd1, (buf + cumread), 50 * sizeof(int), 0); // Should wait about 20msec and return 50*4 bytes
	cumread += read6;
	fprintf(stderr, "6 %d %p\n", (int)cumread, (buf + cumread));
	getnanotime(&ts8); // difference with 7 should be 20msec
	read7 = recv(fd1, (buf + cumread), 250 * sizeof(int), 0); // Shouldn't wait and return (200*4-50*4-445) = 155 bytes, end
	cumread += read7;
	fprintf(stderr, "7 %d %p\n", (int)cumread, (buf + cumread));
	getnanotime(&ts9); // difference with 8 should be 0msec
	read8 = recv(fd1, (buf + cumread), 250 * sizeof(int), 0); // Shouldn't wait and return 0 bytes, end
	cumread += read8;
	fprintf(stderr, "8 %d %p\n", (int)cumread, (buf + cumread));
	read9 = recv(fd1, NULL, 250 * sizeof(int), 0); // Shouldn't wait, not read the buffer, and return 0 bytes, end
	getnanotime(&ts10);
	SANDBOX_END;
	CU_ASSERT_EQUAL(read1, 200*sizeof(int));
	CU_ASSERT_EQUAL(read2, (50+200)*sizeof(int));
	CU_ASSERT_EQUAL(read3, 100*sizeof(int));
	CU_ASSERT_EQUAL(read4, 445);
	CU_ASSERT_EQUAL(read5, 250*sizeof(int));
	CU_ASSERT_EQUAL(read6, 50*sizeof(int));
	CU_ASSERT_EQUAL(read7, ((200-50)*sizeof(int)-445));
	CU_ASSERT_EQUAL(read8, 0);
	CU_ASSERT_EQUAL(read9, 0);
	CU_ASSERT_EQUAL(cumread, 1000 * sizeof(int));
	int64_t diff1 = get_time_interval(&ts1, &ts2),
		diff2 = get_time_interval(&ts2, &ts3),
		diff3 = get_time_interval(&ts3, &ts4),
		diff4 = get_time_interval(&ts4, &ts5),
		diff5 = get_time_interval(&ts5, &ts6),
		diff6 = get_time_interval(&ts6, &ts7),
		diff7 = get_time_interval(&ts7, &ts8),
		diff8 = get_time_interval(&ts8, &ts9),
		diff9 = get_time_interval(&ts9, &ts10);
	fprintf(stderr, "%d %d %d %d %d %d %d %d %d\n", (int)read1, (int)read2, (int)read3, (int)read4, (int)read5, (int)read6, (int)read7, (int)read8, (int)read9);
	fprintf(stderr, "%ld %ld %ld %ld %ld %ld %ld %ld %ld\n", (long)diff1, (long)diff2, (long)diff3, (long)diff4, (long)diff5, (long)diff6, (long)diff7, (long)diff8, (long)diff9);
	/*
	 * TODO should be used to check that the wait times are correct,
	 * but it is extremely machine-dependent.
	CU_ASSERT_TRUE(is_quasi_equal(diff1, 20*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff2, 25*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff3, 0*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff4, 10*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff5, 20*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff6, 15*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff7, 20*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff8, 0*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff9, 0*MILLION));*/
	for (int i = 0; i < 1000; i++) {
		CU_ASSERT_EQUAL(buf1[i], i);
	}
	for (int i = 0; i < 1000; i++) {
		CU_ASSERT_EQUAL(tab1[i], i);
	}
	free_partial_read_buffer(&rbuf1);
	free(tab1);
}

void test_fragmented_recv_after()
{
	set_test_metadata("fragmented_recv_after", _("Tests the use of after-intervals"), 1);
	int64_t MILLION = 1000*1000;
	MONITOR_ALL_RECV(monitored, true);
	reinit_network_socket_stats();
	reinit_read_fd_table();
	int mode = READ_WRITE_AFTER_INTERVAL;
	size_t tab1len = 1000 * sizeof(int);
	int *tab1 = malloc(tab1len);
	if (!tab1)
		CU_FAIL_FATAL("Memory");
	for (int i = 0; i < 1000; i++)
		tab1[i] = i;
	off_t offsets1[] = {250 * sizeof(int), 150 * sizeof(int), 150 * sizeof(int), 200 * sizeof(int), 150 * sizeof(int), 100 * sizeof(int)};
	int intervals1[] = {20, 20, 20, 20, 20, 20};
	struct read_buffer_t rbuf1;
	rbuf1.mode = mode;
	if (create_partial_read_buffer(tab1, 6, offsets1, intervals1, &rbuf1)) {
		free(tab1);
		CU_FAIL_FATAL("Memory");
	}
	int fd1 = 42;
	int r = set_read_data(fd1, &rbuf1);
	if (r < 0) {
		free(tab1);
		free_partial_read_buffer(&rbuf1);
		CU_FAIL_FATAL("Memory");
	}
	CU_ASSERT_EQUAL(r, 0);
	TIMESPEC_ZERO(ts1);
	TIMESPEC_ZERO(ts2);
	TIMESPEC_ZERO(ts3);
	TIMESPEC_ZERO(ts4);
	TIMESPEC_ZERO(ts5);
	TIMESPEC_ZERO(ts6);
	TIMESPEC_ZERO(ts7);
	TIMESPEC_ZERO(ts8);
	TIMESPEC_ZERO(ts9);
	TIMESPEC_ZERO(ts10);
	TIMESPEC_ZERO(ts11);
	ssize_t read1 = 0, read2 = 0, read3 = 0, read4 = 0, read5 = 0, read6 = 0, read7 = 0, read8 = 0, read9 = 0, cumread = 0;
	int errno5 = 0;
	errno = 0;
	int buf1[1010];
	void *buf = buf1;
	SANDBOX_BEGIN;
	getnanotime(&ts1);
	read1 = recv(fd1, (buf + cumread), 200 * sizeof(int), 0); // Wait 20, return 200
	cumread += read1;
	getnanotime(&ts2);
	read2 = recv(fd1, (buf + cumread), 200 * sizeof(int), 0); // No wait, return 50
	cumread += read2;
	getnanotime(&ts3);
	read3 = recv(fd1, (buf + cumread), 200 * sizeof(int), 0); // Wait 20, return 150
	cumread += read3;
	getnanotime(&ts4);
	struct timespec tss1 = (struct timespec) {
		.tv_sec = 0,
		.tv_nsec = 20*MILLION
	};
	nanosleep(&tss1, NULL);
	getnanotime(&ts5);
	read4 = recv(fd1, (buf + cumread), 200 * sizeof(int), 0); // No wait, return 150
	cumread += read4;
	getnanotime(&ts6);
	read5 = recv(fd1, (buf + cumread), 200 * sizeof(int), MSG_DONTWAIT); // No wait, return -1, errno = EAGAIN | EWOULDBLOCK
	errno5 = errno;
	getnanotime(&ts7);
	read6 = recv(fd1, (buf + cumread), 200 * sizeof(int), 0); // Wait, return 200
	cumread += read6;
	getnanotime(&ts8);
	tss1.tv_nsec = 20*MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts9);
	read7 = recv(fd1, (buf + cumread), 200 * sizeof(int), MSG_DONTWAIT); // No wait, return 150
	cumread += read7;
	getnanotime(&ts10);
	read8 = recv(fd1, (buf + cumread), 200 * sizeof(int), 0); // Wait, return 100
	cumread += read8;
	getnanotime(&ts11);
	read9 = recv(fd1, (buf + cumread), 42, 0); // No wait, return 0
	SANDBOX_END;
	CU_ASSERT_EQUAL(read1, 200*sizeof(int));
	CU_ASSERT_EQUAL(read2, 50*sizeof(int));
	CU_ASSERT_EQUAL(read3, 150*sizeof(int));
	CU_ASSERT_EQUAL(read4, 150*sizeof(int));
	CU_ASSERT_EQUAL(read5, -1);
	CU_ASSERT_TRUE(errno5 == EAGAIN || errno == EWOULDBLOCK);
	CU_ASSERT_EQUAL(read6, 200*sizeof(int));
	CU_ASSERT_EQUAL(read7, 150*sizeof(int));
	CU_ASSERT_EQUAL(read8, 100*sizeof(int));
	CU_ASSERT_EQUAL(read9, 0);
	CU_ASSERT_EQUAL(cumread, 1000*sizeof(int));
	fprintf(stderr, "%d %d %d %d %d %d %d %d %d\n", (int)read1, (int)read2, (int)read3, (int)read4, (int)read5, (int)read6, (int)read7, (int)read8, (int)read9);
	int64_t diff1 = get_time_interval(&ts1, &ts2); // 20msec
	int64_t diff2 = get_time_interval(&ts2, &ts3); // 0
	int64_t diff3 = get_time_interval(&ts3, &ts4); // 20
	int64_t diff5 = get_time_interval(&ts5, &ts6); // 0
	int64_t diff6 = get_time_interval(&ts6, &ts7); // 0
	int64_t diff7 = get_time_interval(&ts7, &ts8); // 20
	int64_t diff9 = get_time_interval(&ts9, &ts10); // 0
	int64_t diff10 = get_time_interval(&ts10, &ts11); // 20
	fprintf(stderr, "%ld %ld %ld %ld %ld %ld %ld %ld\n", (long)diff1, (long)diff2, (long)diff3, (long)diff5, (long)diff6, (long)diff7, (long)diff9, (long)diff10);
	/*
	 * TODO Should be used to check wait time,
	 * but is too much machine-dependent.
	CU_ASSERT_TRUE(is_quasi_equal(diff1, 20*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff2, 0*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff3, 20*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff5, 0*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff6, 0*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff7, 20*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff9, 0*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff10, 20*MILLION));*/
	for (int i = 0; i < 1000; i++) {
		CU_ASSERT_EQUAL(buf1[i], i);
		CU_ASSERT_EQUAL(tab1[i], i);
	}
	free_partial_read_buffer(&rbuf1);
	free(tab1);
}

void test_fragmented_recv_realtime()
{
	set_test_metadata("fragmented_recv_realtime", _("Tests the use of real-time-intervals"), 1);
	int64_t MILLION = 1000*1000;
	MONITOR_ALL_RECV(monitored, true);
	monitored.read = true; // Otherwise it won't work
	reinit_network_socket_stats();
	reinit_read_fd_table();
	int mode = READ_WRITE_REAL_INTERVAL;
	size_t tab1len = 1000 * sizeof(int);
	int *tab1 = malloc(tab1len);
	if (!tab1)
		CU_FAIL_FATAL("Memory");
	for (int i = 0; i < 1000; i++)
		tab1[i] = i;
	off_t offsets1[] = {200*sizeof(int), 200*sizeof(int), 200*sizeof(int), 200*sizeof(int), 200*sizeof(int)};
	int intervals1[] = {2*100, 5*100, 1*100, 1*100, 3*100};
	struct read_buffer_t *rbuf1 = create_read_buffer(tab1, 5, offsets1, intervals1, mode);
	if (!rbuf1) {
		free(tab1);
		CU_FAIL_FATAL("Memory");
	}
	int fd1 = 42;
	int r = set_read_data(fd1, rbuf1);
	if (r < 0) {
		free(tab1);
		free_read_buffer(rbuf1);
		CU_FAIL_FATAL("Memory");
	}
	CU_ASSERT_EQUAL(r, 0);
	TIMESPEC_ZERO(tss1);
	TIMESPEC_ZERO(ts0);
	TIMESPEC_ZERO(ts1);
	TIMESPEC_ZERO(ts2);
	TIMESPEC_ZERO(ts3);
	TIMESPEC_ZERO(ts4);
	TIMESPEC_ZERO(ts5);
	TIMESPEC_ZERO(ts6);
	TIMESPEC_ZERO(ts7);
	TIMESPEC_ZERO(ts8);
	TIMESPEC_ZERO(ts9);
	TIMESPEC_ZERO(ts10);
	TIMESPEC_ZERO(ts11);
	TIMESPEC_ZERO(ts12);
	TIMESPEC_ZERO(ts13);
	TIMESPEC_ZERO(ts14);
	TIMESPEC_ZERO(ts15);
	ssize_t read1 = 0, read2 = 0, read3 = 0, read4 = 0, read5 = 0, read6 = 0, read7 = 0, read8 = 0, read9 = 0, cumread = 0;
	int errno3 = 0;
	errno = 0;
	int buf1[1100];
	memset(buf1, 0, sizeof(buf1));
	void *buf = buf1;
	SANDBOX_BEGIN;
	getnanotime(&ts0);

	tss1.tv_nsec = 300 * MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts1);

	read1 = recv(fd1, (buf+cumread), 100*sizeof(int), 0); // No wait, return 100
	cumread += read1;
	getnanotime(&ts2);

	tss1.tv_nsec = 200 * MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts3);

	read2 = read(fd1, (buf+cumread), 150*sizeof(int)); // No wait, return 100
	cumread += read2;
	getnanotime(&ts4);

	read3 = recv(fd1, (buf+cumread), 50*sizeof(int), MSG_DONTWAIT); // No wait, return -1, errno
	errno3 = errno;
	getnanotime(&ts5);

	tss1.tv_nsec = 100 * MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts6);

	read4 = recv(fd1, (buf+cumread), 50*sizeof(int), 0); // Wait 10, return 50
	cumread += read4;
	getnanotime(&ts7);

	tss1.tv_nsec = 200 * MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts8);

	read5 = recv(fd1, (buf+cumread), 100*sizeof(int), 0); // No wait, return 100
	cumread += read5;
	getnanotime(&ts9);

	tss1.tv_nsec = 200 * MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts10);

	read6 = read(fd1, (buf+cumread), 350*sizeof(int)); // No wait, return 350
	cumread += read6; // 700
	getnanotime(&ts11);

	tss1.tv_nsec = 200 * MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts12);

	read7 = recv(fd1, (buf+cumread), 200*sizeof(int), MSG_DONTWAIT); // No wait, return 200
	cumread += read7;
	getnanotime(&ts13);

	tss1.tv_nsec = 200 * MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts14);

	read8 = read(fd1, (buf+cumread), 200*sizeof(int)); // No wait, return 100
	cumread += read8;
	getnanotime(&ts15);

	read9 = read(fd1, (buf+cumread), 200*sizeof(int)); // No wait, return 0
	SANDBOX_END;
	fprintf(stderr, "first test, reads: %d %d %d %d %d %d %d %d %d\n", (int)read1, (int)read2, (int)read3, (int)read4, (int)read5, (int)read6, (int)read7, (int)read8, (int)read9);
	CU_ASSERT_EQUAL(read1, 100*sizeof(int));
	CU_ASSERT_EQUAL(read2, 100*sizeof(int));
	CU_ASSERT_EQUAL(read3, -1);
	CU_ASSERT_TRUE(errno3 == EAGAIN || EWOULDBLOCK);
	CU_ASSERT_EQUAL(read4, 50*sizeof(int));
	CU_ASSERT_EQUAL(read5, 100*sizeof(int));
	CU_ASSERT_EQUAL(read6, 350*sizeof(int));
	CU_ASSERT_EQUAL(read7, 200*sizeof(int));
	CU_ASSERT_EQUAL(read8, 100*sizeof(int));
	CU_ASSERT_EQUAL(read9, 0);
	int64_t diff1 = get_time_interval(&ts1, &ts2); // 0
	int64_t diff2 = get_time_interval(&ts3, &ts4); // 0
	int64_t diff3 = get_time_interval(&ts4, &ts5); // 0
	int64_t diff4 = get_time_interval(&ts6, &ts7); // 10
	int64_t diff5 = get_time_interval(&ts8, &ts9); // 0
	int64_t diff6 = get_time_interval(&ts10, &ts11); // 0
	int64_t diff7 = get_time_interval(&ts12, &ts13); // 0
	int64_t diff8 = get_time_interval(&ts14, &ts15); // 0
	int64_t diff11 = get_time_interval(&ts2, &ts3); // 40
	int64_t diff31 = get_time_interval(&ts5, &ts6); // 10
	int64_t diff41 = get_time_interval(&ts7, &ts8); // 20
	int64_t diff51 = get_time_interval(&ts9, &ts10); // 20
	int64_t diff61 = get_time_interval(&ts11, &ts12); // 20
	int64_t diff71 = get_time_interval(&ts13, &ts14); // 20
	fprintf(stderr, "first test, diff1 %ld %ld %ld %ld diff31 %ld %ld %ld %ld diff51 %ld %ld %ld %ld %ld diff8 %ld\n", diff1, diff11, diff2, diff3, diff31, diff4, diff41, diff5, diff51, diff6, diff61, diff7, diff71, diff8);
	/*
	 * TODO Should be used to check wait time,
	 * but is too much machine-dependent.
	CU_ASSERT_TRUE(diff1 <= MILLION);
	CU_ASSERT_TRUE(diff2 <= MILLION);
	CU_ASSERT_TRUE(diff3 <= MILLION);
	CU_ASSERT_TRUE(diff4 <= 10*MILLION);
	CU_ASSERT_TRUE(diff5 <= MILLION);
	CU_ASSERT_TRUE(diff6 <= MILLION);
	CU_ASSERT_TRUE(diff7 <= MILLION);
	CU_ASSERT_TRUE(diff8 <= MILLION);*/
	for (int i = 0; i < 1000; i++) {
		CU_ASSERT_EQUAL(buf1[i], i);
		CU_ASSERT_EQUAL(tab1[i], i);
	}
	for (int i = 1000; i < 1100; i++)
		CU_ASSERT_EQUAL(buf1[i], 0);
	free_partial_read_buffer(rbuf1);
	// End of test 1
	// Begin of test 2
	off_t offsets2[] = {400*sizeof(int), 400*sizeof(int), 200*sizeof(int)};
	int intervals2[] = {2*100, 4*100, 5*100};
	if (create_partial_read_buffer(tab1, 3, offsets2, intervals2, rbuf1)) {
		free(tab1);
		free(rbuf1);
	}
	if (!fd_is_read_buffered(fd1)) {
		free(tab1);
		free_read_buffer(rbuf1);
		CU_FAIL_FATAL("Memory");
	}
	r = set_read_data(fd1, rbuf1); // Again
	CU_ASSERT_EQUAL(r, 1);
	TS_ZERO(ts0);
	TS_ZERO(ts1);
	TS_ZERO(ts2);
	TS_ZERO(ts3);
	TS_ZERO(ts4);
	TS_ZERO(ts5);
	TS_ZERO(ts6);
	TS_ZERO(ts7);
	TS_ZERO(ts8);
	TS_ZERO(ts9);
	TS_ZERO(ts10);
	TS_ZERO(ts11);
	TS_ZERO(ts12);
	TS_ZERO(ts13);
	TS_ZERO(ts14);
	TS_ZERO(ts15);
	TIMESPEC_ZERO(ts16);
	TIMESPEC_ZERO(ts17);
	TIMESPEC_ZERO(ts18);
	TIMESPEC_ZERO(ts19);
	TIMESPEC_ZERO(ts20);
	TS_ZERO(tss1);
	read1 = 0; read2 = 0; read3 = 0; read4 = 0; read5 = 0; read6 = 0; read7 = 0, read8 = 0, read9 = 0, cumread = 0;
	ssize_t read10 = 0, read11 = 0;
	memset(buf1, 0, sizeof(buf1));
	SANDBOX_BEGIN;
	getnanotime(&ts0);

	tss1.tv_nsec = 100 * MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts1);

	read1 = recv(fd1, (buf+cumread), 100*sizeof(int), 0); // 100, wait 100
	cumread += read1;
	getnanotime(&ts2);

	tss1.tv_nsec = 100 * MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts3);

	read2 = recv(fd1, (buf+cumread), 200*sizeof(int), 0); // 200, wait 0
	cumread += read2;
	getnanotime(&ts4);

	tss1.tv_nsec = 100 * MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts5);

	read3 = recv(fd1, (buf+cumread), 200*sizeof(int), 0); // 100, wait 0
	cumread += read3;
	getnanotime(&ts6);

	tss1.tv_nsec = 100 * MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts7);

	read4 = recv(fd1, (buf+cumread), 100*sizeof(int), 0); // 100, wait 100
	cumread += read4;
	getnanotime(&ts8);

	tss1.tv_nsec = 100 * MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts9);

	read5 = recv(fd1, (buf+cumread), 100*sizeof(int), 0); // 150, wait 0
	cumread += read5;
	getnanotime(&ts10);

	tss1.tv_nsec = 100 * MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts11);

	read6 = read(fd1, (buf+cumread), 100*sizeof(int)); // 150, wait 0
	cumread += read6;
	getnanotime(&ts12);

	tss1.tv_nsec = 100 * MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts13);

	read8 = read(fd1, (buf+cumread), 100*sizeof(int)); // 100, wait 0, emptied
	cumread += read8;
	getnanotime(&ts16);

	tss1.tv_nsec = 100 * MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts17);

	read9 = read(fd1, (buf+cumread), 150*sizeof(int)); // 150, wait 100
	cumread += read9;
	getnanotime(&ts18);

	tss1.tv_nsec = 200 * MILLION;
	nanosleep(&tss1, NULL);
	getnanotime(&ts19);

	read10 = recv(fd1, (buf+cumread), 50*sizeof(int), 0); // 50, wait 0
	cumread += read10;
	getnanotime(&ts20);

	read11 = recv(fd1, (buf+cumread), 50*sizeof(int), 0); // 0, wait 0
	SANDBOX_END;
	fprintf(stderr, "second test, reads %d %d %d %d %d %d %d %d %d %d\n", (int)read1, (int)read2, (int)read3, (int)read4, (int)read5, (int)read6, (int)read8, (int)read9, (int)read10, (int)read11);
	CU_ASSERT_EQUAL(read1, 100*sizeof(int));
	CU_ASSERT_EQUAL(read2, 200*sizeof(int));
	CU_ASSERT_EQUAL(read3, 100*sizeof(int));
	CU_ASSERT_EQUAL(read4, 100*sizeof(int));
	CU_ASSERT_EQUAL(read5, 100*sizeof(int));
	CU_ASSERT_EQUAL(read6, 100*sizeof(int));
	CU_ASSERT_EQUAL(read8, 100*sizeof(int));
	CU_ASSERT_EQUAL(read9, 150*sizeof(int));
	CU_ASSERT_EQUAL(read10, 50*sizeof(int));
	CU_ASSERT_EQUAL(read11, 0);
	diff1 = get_time_interval(&ts1, &ts2);
	diff2 = get_time_interval(&ts3, &ts4);
	diff3 = get_time_interval(&ts5, &ts6);
	diff4 = get_time_interval(&ts7, &ts8);
	diff5 = get_time_interval(&ts9, &ts10);
	diff6 = get_time_interval(&ts11, &ts12);
	diff7 = get_time_interval(&ts13, &ts16);
	int64_t diff9 = get_time_interval(&ts17, &ts18);
	int64_t diff10 = get_time_interval(&ts19, &ts20);
	fprintf(stderr, "second test, diff1 %ld %ld %ld diff4 %ld %ld %ld diff7 %ld %ld diff10 %ld\n", (long)diff1, (long)diff2, (long)diff3, (long)diff4, (long)diff5, (long)diff6, (long)diff7, (long)diff9, (long)diff10);
	/*
	 * TODO Should be used to check wait time,
	 * but is machine-dependent.
	CU_ASSERT_TRUE(is_quasi_equal(diff1, 10*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff2, 0*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff3, 0*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff4, 10*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff5, 0*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff6, 0*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff7, 0*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff8, 0*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff9, 10*MILLION));
	CU_ASSERT_TRUE(is_quasi_equal(diff10, 0*MILLION)); */
	for (int i = 0; i < 1000; i++) {
		//fprintf(stderr, "%d %d %d\n", i, tab1[i], buf1[i]);
		CU_ASSERT_EQUAL(buf1[i], i);
		CU_ASSERT_EQUAL(tab1[i], i);
	}
	for (int i = 1000; i < 1100; i++)
		CU_ASSERT_EQUAL(buf1[i], 0);
	free_partial_read_buffer(rbuf1);
	// End of test 2
	// Begin of test 3
	off_t offsets3[10] = {250*sizeof(int), 100*sizeof(int), 100*sizeof(int), 100*sizeof(int), 100*sizeof(int), 100*sizeof(int), 100*sizeof(int), 50*sizeof(int), 50*sizeof(int), 50*sizeof(int)};
	int intervals3[10] = {2*50, 4*50, 6*50, 1*50, 1*50, 1*50, 2*50, 3*50, 3*50, 1*50};
	if (create_partial_read_buffer(tab1, 10, offsets3, intervals3, rbuf1)) {
		free(tab1);
		free(rbuf1);
	}
	r = set_read_data(fd1, rbuf1);
	CU_ASSERT_EQUAL_FATAL(r, 1);
	// TODO check time interval
	TS_ZERO(tss1);
	read1 = 0; read2 = 0; read3 = 0; read4 = 0; read5 = 0; read6 = 0;
	read7 = 0; read8 = 0; read9 = 0; read10 = 0; read11 = 0; cumread = 0;
	ssize_t read12 = 0, read13 = 0, read14 = 0;
	memset(buf1, 0, sizeof(buf1));
	SANDBOX_BEGIN;
	tss1.tv_nsec = 3*50*MILLION;
	nanosleep(&tss1, NULL);

	read1 = recv(fd1, (buf+cumread), 100*sizeof(int), 0);
	cumread += read1;

	tss1.tv_nsec = 2*50*MILLION;
	nanosleep(&tss1, NULL);

	read2 = recv(fd1, (buf+cumread), 100*sizeof(int), 0);
	cumread += read2;

	tss1.tv_nsec = 2*50*MILLION;
	nanosleep(&tss1, NULL);

	read3 = recv(fd1, (buf+cumread), 100*sizeof(int), 0);
	cumread += read3;

	tss1.tv_nsec = 2*50*MILLION;
	nanosleep(&tss1, NULL);

	read4 = recv(fd1, (buf+cumread), 100*sizeof(int), 0); // 50
	cumread += read4;

	tss1.tv_nsec = 2*50*MILLION;
	nanosleep(&tss1, NULL);

	read5 = recv(fd1, (buf+cumread), 50*sizeof(int), 0); // wait 1*50
	cumread += read5;

	tss1.tv_nsec = 2*50*MILLION;
	nanosleep(&tss1, NULL);

	read6 = recv(fd1, (buf+cumread), 20*sizeof(int), 0);
	cumread += read6;

	tss1.tv_nsec = 1*50*MILLION;
	nanosleep(&tss1, NULL);

	read7 = recv(fd1, (buf+cumread), (30+50)*sizeof(int), 0);
	cumread += read7;

	tss1.tv_nsec = 1*50*MILLION;
	nanosleep(&tss1, NULL);

	read8 = recv(fd1, (buf+cumread), 200*sizeof(int), 0);
	cumread += read8;

	tss1.tv_nsec = 2*50*MILLION;
	nanosleep(&tss1, NULL);

	read9 = recv(fd1, (buf+cumread), 50*sizeof(int), 0);
	cumread += read9;

	tss1.tv_nsec = 1*50*MILLION;
	nanosleep(&tss1, NULL);

	read10 = recv(fd1, (buf+cumread), 100*sizeof(int), 0);
	cumread += read10;

	tss1.tv_nsec = 2*50*MILLION;
	nanosleep(&tss1, NULL);

	read11 = recv(fd1, (buf+cumread), 100*sizeof(int), 0); // 50
	cumread += read11;

	tss1.tv_nsec = 1*50*MILLION;
	nanosleep(&tss1, NULL);

	read12 = recv(fd1, (buf+cumread), 100*sizeof(int), 0); // 50, wait 1*50
	cumread += read12;

	tss1.tv_nsec = 1*50*MILLION;
	nanosleep(&tss1, NULL);

	read13 = recv(fd1, (buf+cumread), 50*sizeof(int), 0);
	cumread += read13;

	read14 = recv(fd1, (buf+cumread), 50*sizeof(int), 0);
	cumread += read14;
	SANDBOX_END;
	fprintf(stderr, "third test, reads %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
			(int)read1, (int)read2, (int)read3, (int)read4, (int)read5,
			(int)read6, (int)read7, (int)read8, (int)read9, (int)read10,
			(int)read11, (int)read12, (int)read13, (int)read14);
	CU_ASSERT_EQUAL(read1, 100*sizeof(int));
	CU_ASSERT_EQUAL(read2, 100*sizeof(int));
	CU_ASSERT_EQUAL(read3, 100*sizeof(int));
	CU_ASSERT_EQUAL(read4, 50*sizeof(int));
	CU_ASSERT_EQUAL(read5, 50*sizeof(int));
	CU_ASSERT_EQUAL(read6, 20*sizeof(int));
	CU_ASSERT_EQUAL(read7, 80*sizeof(int));
	CU_ASSERT_EQUAL(read8, 200*sizeof(int));
	CU_ASSERT_EQUAL(read9, 50*sizeof(int));
	CU_ASSERT_EQUAL(read10, 100*sizeof(int));
	CU_ASSERT_EQUAL(read11, 50*sizeof(int));
	CU_ASSERT_EQUAL(read12, 50*sizeof(int));
	CU_ASSERT_EQUAL(read13, 50*sizeof(int));
	CU_ASSERT_EQUAL(read14, 0*sizeof(int));
	for (int i = 0; i < 1000; i++) {
		CU_ASSERT_EQUAL(i, buf1[i]);
		CU_ASSERT_EQUAL(i, tab1[i]);
	}
	for (int i = 1000; i < 1100; i++)
		CU_ASSERT_EQUAL(buf1[i], 0);
	free_read_buffer(rbuf1);
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

