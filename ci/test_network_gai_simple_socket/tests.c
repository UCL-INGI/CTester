#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "CTester/CTester.h"
#include "student_code.h"

void __real_exit(int status); // Needed as otherwise we'll get a segfault

void test_wrapper_stats()
{
	// Let's enable the stats for all functions
	set_test_metadata("wrapper_stats", _("Tests that the wrapper functions correctly remembers the stats"), 1);
	CU_ASSERT_EQUAL(stats.accept.called, 0);
	CU_ASSERT_EQUAL(stats.bind.called, 0);
	CU_ASSERT_EQUAL(stats.connect.called, 0);
	CU_ASSERT_EQUAL(stats.listen.called, 0);
	CU_ASSERT_EQUAL(stats.recv.called, 0);
	CU_ASSERT_EQUAL(stats.recvfrom.called, 0);
	CU_ASSERT_EQUAL(stats.recvmsg.called, 0);
	CU_ASSERT_EQUAL(stats.recv_all.called, 0);
	CU_ASSERT_EQUAL(stats.send.called, 0);
	CU_ASSERT_EQUAL(stats.sendto.called, 0);
	CU_ASSERT_EQUAL(stats.sendmsg.called, 0);
	CU_ASSERT_EQUAL(stats.send_all.called, 0);
	CU_ASSERT_EQUAL(stats.socket.called, 0);

	CU_ASSERT_EQUAL(stats.accept.last_return, 0);
	CU_ASSERT_EQUAL(stats.bind.last_return, 0);
	CU_ASSERT_EQUAL(stats.connect.last_return, 0);
	CU_ASSERT_EQUAL(stats.listen.last_return, 0);
	CU_ASSERT_EQUAL(stats.recv.last_return, 0);
	CU_ASSERT_EQUAL(stats.recvfrom.last_return, 0);
	CU_ASSERT_EQUAL(stats.recvmsg.last_return, 0);
	CU_ASSERT_EQUAL(stats.send.last_return, 0);
	CU_ASSERT_EQUAL(stats.sendto.last_return, 0);
	CU_ASSERT_EQUAL(stats.sendmsg.last_return, 0);
	CU_ASSERT_EQUAL(stats.socket.last_return, 0);

	monitored.getaddrinfo = monitored.freeaddrinfo = true;
	monitored.accept = monitored.bind = monitored.connect = monitored.listen = monitored.socket = true;
	MONITOR_ALL_RECV(monitored, true);
	MONITOR_ALL_SEND(monitored, true);
	CU_ASSERT_EQUAL(monitored.recv, true);
	CU_ASSERT_EQUAL(monitored.recvfrom, true);
	CU_ASSERT_EQUAL(monitored.recvmsg, true);
	CU_ASSERT_EQUAL(monitored.send, true);
	CU_ASSERT_EQUAL(monitored.sendto, true);
	CU_ASSERT_EQUAL(monitored.sendmsg, true);

	// wrap_monitoring is set to true inside the sandbox, and set to false at the end of it.
	struct f1_stats stats1;
	SANDBOX_BEGIN;
	run_student_tests_wrapper_stats_1(&stats1);
	SANDBOX_END;

	CU_ASSERT_EQUAL(stats.accept.called, 0);
	CU_ASSERT_EQUAL(stats.bind.called, 0);
	CU_ASSERT_EQUAL(stats.connect.called, stats1.nb_connect);
	CU_ASSERT_EQUAL(stats.listen.called, 0);
	CU_ASSERT_EQUAL(stats.recv.called, 0);
	CU_ASSERT_EQUAL(stats.recvfrom.called, 0);
	CU_ASSERT_EQUAL(stats.recvmsg.called, 0);
	CU_ASSERT_EQUAL(stats.recv_all.called, 0);
	CU_ASSERT_EQUAL(stats.send.called, stats1.nb_send);
	CU_ASSERT_EQUAL(stats.sendto.called, 0);
	CU_ASSERT_EQUAL(stats.sendmsg.called, 0);
	CU_ASSERT_EQUAL(stats.send_all.called, stats1.nb_send);
	CU_ASSERT_EQUAL(stats.socket.called, stats1.nb_addr_tried);

	reinit_stats_network_dns();
	reinit_network_socket_stats(); // Simpler for the next calls

	int pid;
	pid = fork();
	if (pid == 0) {
		struct stats2_server statss;
		SANDBOX_BEGIN;
		run_student_tests_wrapper_stats_2_server(&statss);
		SANDBOX_END;
		// I must exit to let the other process terminate. No test here !
		__real_exit(0);
	} else if (pid != -1) {
		struct stats2_client statss;
		memset(&statss, 0, sizeof(statss));
		sleep(1); // Strangely extremely important FIXME
		SANDBOX_BEGIN;
		run_student_tests_wrapper_stats_2_client(&statss);
		SANDBOX_END;
		int status = 0;
		waitpid(pid, &status, 0);
		// Let's have a look at the statistics for the client
		CU_ASSERT_EQUAL(stats.socket.called, statss.nsocket);
		CU_ASSERT_EQUAL(stats.connect.called, statss.nconnect);
		CU_ASSERT_EQUAL(stats.getaddrinfo.called, statss.ngai);
		CU_ASSERT_EQUAL(stats.freeaddrinfo.called, (statss.ngai > 0 && statss.nsocket > 0));
		CU_ASSERT_EQUAL(stats.sendmsg.called, statss.nsend);
		CU_ASSERT_EQUAL(stats.send_all.called, statss.nsend);
		CU_ASSERT_EQUAL(stats.recv.called, statss.nrecv);
		CU_ASSERT_EQUAL(stats.recv_all.called, statss.nrecv);
	} else {
		CU_FAIL();
	}

	reinit_stats_network_dns();
	reinit_network_socket_stats();

	pid = fork();
	if (pid == 0) {
		struct stats2_client statss;
		SANDBOX_BEGIN;
		run_student_tests_wrapper_stats_2_client(&statss);
		SANDBOX_END;
		// I must exit to let the other process terminate. No test here !
		__real_exit(0);
	} else if (pid != -1) {
		struct stats2_server statss;
		memset(&statss, 0, sizeof(statss));
		SANDBOX_BEGIN;
		run_student_tests_wrapper_stats_2_server(&statss);
		SANDBOX_END;
		int status = 0;
		waitpid(pid, &status, 0);
		// Let's have a look at the statistics for the server
		CU_ASSERT_EQUAL(stats.bind.called, statss.nbind);
		CU_ASSERT_EQUAL(stats.socket.called, statss.nsocket);
		CU_ASSERT_EQUAL(stats.getaddrinfo.called, statss.ngai);
		CU_ASSERT_EQUAL(stats.sendto.called, statss.nsendto);
		CU_ASSERT_EQUAL(stats.recvfrom.called, statss.nrecvfrom);
		CU_ASSERT_EQUAL(stats.recv_all.called, statss.nrecvfrom);
		CU_ASSERT_EQUAL(stats.send_all.called, statss.nsendto);
	} else {
		CU_FAIL();
	}

	monitored.getaddrinfo = monitored.freeaddrinfo = false;
	monitored.accept = monitored.bind = monitored.connect = monitored.listen = monitored.socket = false;
	MONITOR_ALL_RECV(monitored, false);
	MONITOR_ALL_SEND(monitored, false);
	CU_ASSERT_EQUAL(monitored.recv, false);
	CU_ASSERT_EQUAL(monitored.recvfrom, false);
	CU_ASSERT_EQUAL(monitored.recvmsg, false);
	CU_ASSERT_EQUAL(monitored.send, false);
	CU_ASSERT_EQUAL(monitored.sendto, false);
	CU_ASSERT_EQUAL(monitored.sendmsg, false);

	reinit_stats_network_dns();
	reinit_network_socket_stats();
	CU_ASSERT_EQUAL(stats.accept.called, 0);
	CU_ASSERT_EQUAL(stats.bind.called, 0);
	CU_ASSERT_EQUAL(stats.connect.called, 0);
	CU_ASSERT_EQUAL(stats.listen.called, 0);
	CU_ASSERT_EQUAL(stats.recv.called, 0);
	CU_ASSERT_EQUAL(stats.recvfrom.called, 0);
	CU_ASSERT_EQUAL(stats.recvmsg.called, 0);
	CU_ASSERT_EQUAL(stats.recv_all.called, 0);
	CU_ASSERT_EQUAL(stats.send.called, 0);
	CU_ASSERT_EQUAL(stats.sendto.called, 0);
	CU_ASSERT_EQUAL(stats.sendmsg.called, 0);
	CU_ASSERT_EQUAL(stats.send_all.called, 0);
	CU_ASSERT_EQUAL(stats.socket.called, 0);

	CU_ASSERT_EQUAL(stats.accept.last_return, 0);
	CU_ASSERT_EQUAL(stats.bind.last_return, 0);
	CU_ASSERT_EQUAL(stats.connect.last_return, 0);
	CU_ASSERT_EQUAL(stats.listen.last_return, 0);
	CU_ASSERT_EQUAL(stats.recv.last_return, 0);
	CU_ASSERT_EQUAL(stats.recvfrom.last_return, 0);
	CU_ASSERT_EQUAL(stats.recvmsg.last_return, 0);
	CU_ASSERT_EQUAL(stats.send.last_return, 0);
	CU_ASSERT_EQUAL(stats.sendto.last_return, 0);
	CU_ASSERT_EQUAL(stats.sendmsg.last_return, 0);
	CU_ASSERT_EQUAL(stats.socket.last_return, 0);
}

// Check failures:
// Recall we must use the failures struct, and assign its (call), (call)_ret and (call)_errno fields.
// And we have the fileds FAIL_ALWAYS, FAIL_NEVER, FAIL_FIRST, FAIL_SECOND, FAIL_THIRD and FAIL_TWICE

int main(int argc, char **argv)
{
	RUN(test_wrapper_stats);
}

