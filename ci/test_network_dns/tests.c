#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#include "CTester/CTester.h"
#include "student_code.h"

int __real_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
int __real_getnameinfo(const struct sockaddr *addr, socklen_t addrlen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags);
void __real_freeaddrinfo(struct addrinfo *res);
const char *__real_gai_strerror(int errcode);

const char gai1_cname[] = "CGAI1";
const char gai2_cname[] = "CGAI2";
struct custom_gai1_stats_t {
	int called;
	const char *node;
	const char *serv;
	const struct addrinfo *hints;
	struct addrinfo **res;
} custom_gai1_stats;
int custom_gai1(const char *node, const char *serv, const struct addrinfo *hints, struct addrinfo **res)
{
	custom_gai1_stats.called++;
	custom_gai1_stats.node = node;
	custom_gai1_stats.serv = serv;
	custom_gai1_stats.hints = hints;
	custom_gai1_stats.res = res;
	*res = calloc(1, sizeof(struct addrinfo));
	(*res)->ai_family = AF_INET;
	(*res)->ai_socktype = SOCK_DGRAM;
	(*res)->ai_protocol = 0;
	(*res)->ai_addrlen = 0;
	(*res)->ai_addr = NULL;
	(*res)->ai_canonname = NULL;
	(*res)->ai_next = NULL;
	if (node != NULL) {
		return 0;
	} else if (serv == NULL) {
		return 1;
	} else {
		return 2;
	}
}

struct custom_gai2_stats_t {
	int called;
	const char *node;
	const char *serv;
	const struct addrinfo *hints;
	struct addrinfo **res;
} custom_gai2_stats;
int custom_gai2(const char *node, const char *serv, const struct addrinfo *hints, struct addrinfo **res)
{
	custom_gai2_stats.called++;
	custom_gai2_stats.node = node;
	custom_gai2_stats.serv = serv;
	custom_gai2_stats.hints = hints;
	custom_gai2_stats.res = res;
	return __real_getaddrinfo(node, serv, hints, res);
}

struct custom_fai1_stats_t {
	int called;
	struct addrinfo *res;
	struct addrinfo *no_free;
} custom_fai1_stats;
void custom_fai1(struct addrinfo *res)
{
	custom_fai1_stats.called++;
	custom_fai1_stats.res = res;
	if (custom_fai1_stats.no_free != res) {
		free(res);
	}
}
void custom_fai1_prevent_free(struct addrinfo *res)
{
	custom_fai1_stats.no_free = res;
}

struct custom_fai2_stats_t {
	int called;
	struct addrinfo *res;
} custom_fai2_stats;
void custom_fai2(struct addrinfo *res)
{
	custom_fai2_stats.called++;
	custom_fai2_stats.res = res;
	__real_freeaddrinfo(res);
}

struct custom_gni1_stats_t {
	int called;
	const struct sockaddr *addr;
	socklen_t addrlen;
	socklen_t hostlen;
	socklen_t servlen;
	int flags;
	char *host;
	char *serv;
} custom_gni1_stats;
int custom_gn1(const struct sockaddr *addr, socklen_t addrlen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags)
{
	custom_gni1_stats.called++;
	custom_gni1_stats.addr = addr;
	custom_gni1_stats.addrlen = addrlen;
	custom_gni1_stats.host = host;
	custom_gni1_stats.hostlen = hostlen;
	custom_gni1_stats.serv = serv;
	custom_gni1_stats.servlen = servlen;
	custom_gni1_stats.flags = flags;
	char *s1 = "Hello";
	char *s2 = "world";
	memcpy(host, s1, 6);
	memcpy(serv, s2, 6);
	return 0;
}

struct custom_gni2_stats_t {
	int called;
	const struct sockaddr *addr;
	socklen_t addrlen;
	socklen_t hostlen;
	socklen_t servlen;
	int flags;
	char *host;
	char *serv;
} custom_gni2_stats;
int custom_gn2(const struct sockaddr *addr, socklen_t addrlen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags)
{
	custom_gni2_stats.called++;
	custom_gni2_stats.addr = addr;
	custom_gni2_stats.addrlen = addrlen;
	custom_gni2_stats.host = host;
	custom_gni2_stats.hostlen = hostlen;
	custom_gni2_stats.serv = serv;
	custom_gni2_stats.servlen = servlen;
	custom_gni2_stats.flags = flags;
	return __real_getnameinfo(addr, addrlen, host, hostlen, serv, servlen, flags);
}

struct custom_fai_reporter_stats_t {
	int called;
} custom_fair_stats;
void custom_checked_fai_reporter()
{
	custom_fair_stats.called++;
}

struct custom_gse1_stats_t {
	int called;
	int code;
} custom_gse1_stats;
const char *custom_gai_strerror1(int code)
{
	custom_gse1_stats.called++;
	custom_gse1_stats.code = code;
	char *s1 = "GSE1";
	return s1;
}

struct custom_gse2_stats_t {
	int called;
	int code;
} custom_gse2_stats;
const char *custom_gai_strerror2(int code)
{
	custom_gse2_stats.called++;
	custom_gse2_stats.code = code;
	char *s2 = "GSE2";
	return s2;
}

/*
What to test:
- set_gai works, as well as none (from none)
- set_gais all, gai, fai, none (null) work
- set_gai and then reset it works
- set_gais and then reset both works
- set_gni works, both set and reset
- set_gai_strerror should work, as well as reset
- checked freeaddrinfo should work: activate, pass a correct, pass an incorrect, deactivate, pass an incorrect
- the reporter for the checked fai should also work, both set and reset.
 */
void test_gai_substitutes()
{
	set_test_metadata("substitute_gai", _("Test if the substitution functions etc work correctly"), 1);
	reinit_all_monitored();
	reinit_all_stats();
	reset_gai_fai_gstr_gni_methods();
	memset(&custom_gai1_stats, 0, sizeof(custom_gai1_stats));
	memset(&custom_gai2_stats, 0, sizeof(custom_gai2_stats));
	memset(&custom_fai1_stats, 0, sizeof(custom_fai1_stats));
	memset(&custom_fai2_stats, 0, sizeof(custom_fai2_stats));
	memset(&custom_gni1_stats, 0, sizeof(custom_gni1_stats));
	memset(&custom_gni2_stats, 0, sizeof(custom_gni2_stats));
	memset(&custom_fair_stats, 0, sizeof(custom_fair_stats));
	memset(&custom_gse1_stats, 0, sizeof(custom_gse1_stats));
	memset(&custom_gse2_stats, 0, sizeof(custom_gse2_stats));
	set_check_freeaddrinfo(false); // Disable it for all tests

	int i1=-1, i2=-1, i3=-1, i4=-1, i5=-1, i6=-1, i7=-1, i8=-1, i9=-1;
	struct addrinfo hints1, *res1, *res2, *res3, *res4, *res5, *res6, *res7, *res8, *res9;
	memset(&hints1, 0, sizeof(hints1));
	hints1.ai_family = AF_INET;
	hints1.ai_socktype = SOCK_DGRAM;
	hints1.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
	char h1[]="0.0.0.1", h2[]="0.0.0.2", h3[]="0.0.0.3", h4[]="0.0.0.4";
	char h5[]="0.0.0.5", h6[]="0.0.0.6", h7[]="0.0.0.7", h8[]="0.0.0.8";
	(void)h8;
	(void)res8;
	(void)res9;
	(void)i9;
	monitored.getaddrinfo = monitored.freeaddrinfo = false;
	SANDBOX_BEGIN;
	i1 = getaddrinfo(h1, "80", &hints1, &res1);
	SANDBOX_END;
	CU_ASSERT_EQUAL(i1, 0);
	CU_ASSERT_PTR_NOT_NULL(res1);
	CU_ASSERT_EQUAL(stats.getaddrinfo.called, 0); // Not enabled yet
	SANDBOX_BEGIN;
	freeaddrinfo(res1);
	SANDBOX_END;
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 0); // Not enabled yet
	monitored.getaddrinfo = monitored.freeaddrinfo = true;
	i1=-1;
	res1 = NULL;
	SANDBOX_BEGIN;
	i1 = getaddrinfo(h2, "80", &hints1, &res1);
	freeaddrinfo(res1);
	SANDBOX_END;
	CU_ASSERT_EQUAL(stats.getaddrinfo.called, 1);
	CU_ASSERT_EQUAL(stats.getaddrinfo.last_params.res, &res1);
	CU_ASSERT_EQUAL(stats.getaddrinfo.last_params.hints, &hints1);
	CU_ASSERT_EQUAL(stats.getaddrinfo.last_params.node, h2);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 1);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.last_param, res1);
	// Now, let's set getaddrinfo to someone else
	set_getaddrinfo_method(custom_gai2); // That's just a wrapper, compatible with __real_freeaddrinfo
	SANDBOX_BEGIN;
	i2 = getaddrinfo(h3, "80", &hints1, &res2);
	freeaddrinfo(res2);
	SANDBOX_END;
	CU_ASSERT_EQUAL(stats.getaddrinfo.called, 2);
	CU_ASSERT_EQUAL(stats.getaddrinfo.last_params.node, h3);
	CU_ASSERT_EQUAL(stats.getaddrinfo.last_params.hints, &hints1);
	CU_ASSERT_EQUAL(stats.getaddrinfo.last_params.res, &res2);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 2);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.last_param, res2);
	CU_ASSERT_EQUAL(custom_gai2_stats.called, 1);
	CU_ASSERT_EQUAL(custom_gai2_stats.node, h3);
	CU_ASSERT_EQUAL(custom_gai2_stats.res, &res2);
	res2 = NULL;
	CU_ASSERT_EQUAL(getaddrinfo(h3, "80", &hints1, &res2), 0);
	CU_ASSERT_EQUAL(stats.getaddrinfo.called, 2); // No change
	CU_ASSERT_EQUAL(custom_gai2_stats.called, 1); // Neither
	set_getaddrinfo_method(NULL);
	SANDBOX_BEGIN;
	i3 = getaddrinfo(h4, "80", &hints1, &res3);
	freeaddrinfo(res3);
	SANDBOX_END;
	CU_ASSERT_EQUAL(stats.getaddrinfo.called, 3);
	CU_ASSERT_EQUAL(custom_gai2_stats.called, 1);
	CU_ASSERT_EQUAL(custom_fai2_stats.called, 0);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 3);
	set_gai_methods(custom_gai1, custom_fai1);
	SANDBOX_BEGIN;
	i4 = getaddrinfo(h5, "80", &hints1, &res4);
	freeaddrinfo(res4);
	SANDBOX_END;
	CU_ASSERT_EQUAL(i4, 0);
	CU_ASSERT_EQUAL(stats.getaddrinfo.called, 4);
	CU_ASSERT_EQUAL(custom_gai1_stats.called, 1);
	CU_ASSERT_EQUAL(custom_gai1_stats.node, h5);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 4);
	CU_ASSERT_EQUAL(custom_fai1_stats.called, 1);
	CU_ASSERT_EQUAL(custom_fai1_stats.res, res4);
	CU_ASSERT_EQUAL(getaddrinfo(h4, "80", &hints1, &res3), 0);
	CU_ASSERT_EQUAL(stats.getaddrinfo.called, 4);
	CU_ASSERT_EQUAL(custom_gai1_stats.called, 1);
	freeaddrinfo(res3);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 4);
	CU_ASSERT_EQUAL(custom_fai1_stats.called, 1);
	set_gai_methods(custom_gai2, custom_fai2);
	SANDBOX_BEGIN;
	i5 = getaddrinfo(h6, "80", &hints1, &res5);
	freeaddrinfo(res5);
	SANDBOX_END;
	CU_ASSERT_EQUAL(i5, 0);
	CU_ASSERT_EQUAL(stats.getaddrinfo.called, 5);
	CU_ASSERT_EQUAL(custom_gai2_stats.called, 2);
	CU_ASSERT_EQUAL(custom_fai2_stats.called, 1);
	set_gai_methods(NULL, custom_fai2);
	SANDBOX_BEGIN;
	i6 = getaddrinfo(h5, "80", &hints1, &res6);
	freeaddrinfo(res6);
	SANDBOX_END;
	CU_ASSERT_EQUAL(i6, 0);
	CU_ASSERT_EQUAL(stats.getaddrinfo.called, 6);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 6);
	CU_ASSERT_EQUAL(custom_gai2_stats.called, 2);
	CU_ASSERT_EQUAL(custom_fai2_stats.called, 2);
	set_gai_methods(custom_gai2, NULL);
	i5=-1;
	SANDBOX_BEGIN;
	i5 = getaddrinfo(h6, "80", &hints1, &res5);
	freeaddrinfo(res5);
	SANDBOX_END;
	CU_ASSERT_EQUAL(i5, 0);
	CU_ASSERT_EQUAL(stats.getaddrinfo.called, 7);
	CU_ASSERT_EQUAL(custom_gai2_stats.called, 3);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 7);
	CU_ASSERT_EQUAL(custom_fai2_stats.called, 2);
	set_gai_methods(custom_gai2, custom_fai2);
	monitored.getaddrinfo = false;
	monitored.freeaddrinfo = true;
	SANDBOX_BEGIN;
	i6 = getaddrinfo(h5, "80", &hints1, &res6);
	freeaddrinfo(res6);
	SANDBOX_END;
	CU_ASSERT_EQUAL(i6, 0);
	CU_ASSERT_EQUAL(stats.getaddrinfo.called, 7);
	CU_ASSERT_EQUAL(custom_gai2_stats.called, 3);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 8);
	CU_ASSERT_EQUAL(custom_fai2_stats.called, 3);
	monitored.getaddrinfo = true;
	monitored.freeaddrinfo = false;
	SANDBOX_BEGIN;
	i7 = getaddrinfo(h7, "80", &hints1, &res7);
	freeaddrinfo(res7);
	SANDBOX_END;
	CU_ASSERT_EQUAL(stats.getaddrinfo.called, 8);
	CU_ASSERT_EQUAL(custom_gai2_stats.called, 4);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 8);
	CU_ASSERT_EQUAL(custom_fai2_stats.called, 3)	;
	// TODO should also test that it doesn't do anything else

	// Testing getnameinfo
	CU_ASSERT_EQUAL(stats.getnameinfo.called, 0);
	i1 = i2 = i3 = i4 = i5 = i6 = i7 = i8 = -1;
	int flags = NI_NUMERICHOST;
	struct sockaddr_in s1, s2, s3, s4;
	memset(&s1, 0, sizeof(s1));
	memset(&s2, 0, sizeof(s2));
	memset(&s3, 0, sizeof(s3));
	memset(&s4, 0, sizeof(s4));
	s1.sin_family = s2.sin_family = s3.sin_family = s4.sin_family = AF_INET;
	s1.sin_port = s2.sin_port = s3.sin_port = s4.sin_port = htons(80);
	inet_pton(AF_INET, "0.0.0.1", &(s1.sin_addr));
	inet_pton(AF_INET, "0.0.0.2", &(s2.sin_addr));
	inet_pton(AF_INET, "0.0.0.3", &(s3.sin_addr));
	inet_pton(AF_INET, "0.0.0.4", &(s4.sin_addr));
	CU_ASSERT_EQUAL(s1.sin_addr.s_addr, htonl(1));
	CU_ASSERT_EQUAL(s2.sin_addr.s_addr, htonl(2));
	CU_ASSERT_EQUAL(s3.sin_addr.s_addr, htonl(3));
	CU_ASSERT_EQUAL(s4.sin_addr.s_addr, htonl(4));
	char rh1[10]={0}, rh2[10]={0}, rh3[10]={0}, rh4[10]={0};
	char rs1[10]={0}, rs2[10]={0}, rs3[10]={0}, rs4[10]={0};
	// Tests
	i1 = getnameinfo((struct sockaddr*)&s1, sizeof(s1), rh1, sizeof(rh1), rs1, sizeof(rs1), flags);
	CU_ASSERT_EQUAL(stats.getnameinfo.called, 0);
	CU_ASSERT_EQUAL(strcmp(rh1, "0.0.0.1"), 0);
	SANDBOX_BEGIN;
	i2 = getnameinfo((struct sockaddr*)&s2, sizeof(s2), rh2, sizeof(rh2), rs2, sizeof(rs2), flags);
	SANDBOX_END;
	CU_ASSERT_EQUAL(stats.getnameinfo.called, 0);
	CU_ASSERT_EQUAL(strcmp(rh2, "0.0.0.2"), 0);
	monitored.getnameinfo = true;
	SANDBOX_BEGIN;
	i3 = getnameinfo((struct sockaddr*)&s3, sizeof(s3), rh3, sizeof(rh3), rh4, sizeof(rh4), flags);
	SANDBOX_END;
	CU_ASSERT_EQUAL(stats.getnameinfo.called, 1);
	CU_ASSERT_EQUAL(strcmp(rh3, "0.0.0.3"), 0);
	set_getnameinfo_method(custom_gn1);
	CU_ASSERT_EQUAL(custom_gni1_stats.called, 0);
	SANDBOX_BEGIN;
	i4 = getnameinfo((struct sockaddr*)&s4, sizeof(s4), rh4, sizeof(rh4), rs4, sizeof(rs4), flags);
	SANDBOX_END;
	CU_ASSERT_EQUAL(stats.getnameinfo.called, 2);
	CU_ASSERT_EQUAL(custom_gni1_stats.called, 1);
	CU_ASSERT_EQUAL(custom_gni1_stats.addr, &s4);
	CU_ASSERT_EQUAL(strcmp(rh4, "Hello"), 0);
	monitored.getnameinfo = false;
	SANDBOX_BEGIN;
	i5 = getnameinfo((struct sockaddr*)&s1, sizeof(s1), rh1, sizeof(rh1), rs1, sizeof(rs1), flags);
	SANDBOX_END;
	CU_ASSERT_EQUAL(stats.getnameinfo.called, 2);
	CU_ASSERT_EQUAL(custom_gni1_stats.called, 1);
	CU_ASSERT_EQUAL(custom_gni1_stats.addr, &s4);
	monitored.getnameinfo = true;
	SANDBOX_BEGIN;
	i6 = getnameinfo((struct sockaddr*)&s2, sizeof(s2), rh2, sizeof(rh2), rs2, sizeof(rs2), flags);
	SANDBOX_END;
	CU_ASSERT_EQUAL(stats.getnameinfo.called, 3);
	CU_ASSERT_EQUAL(stats.getnameinfo.last_params.addr, &s2);
	CU_ASSERT_EQUAL(custom_gni1_stats.called, 2);
	CU_ASSERT_EQUAL(custom_gni1_stats.addr, &s2);
	set_getnameinfo_method(NULL);
	SANDBOX_BEGIN;
	i7 = getnameinfo((struct sockaddr*)&s3, sizeof(s3), rh3, sizeof(rh3), rs3, sizeof(rs3), flags);
	SANDBOX_END;
	CU_ASSERT_EQUAL(stats.getnameinfo.called, 4);
	CU_ASSERT_EQUAL(stats.getnameinfo.last_params.addr, &s3);
	CU_ASSERT_EQUAL(custom_gni1_stats.called, 2);
	CU_ASSERT_EQUAL(custom_gni1_stats.addr, &s2);
	set_getnameinfo_method(custom_gn2);
	CU_ASSERT_EQUAL(custom_gni2_stats.called, 0);
	SANDBOX_BEGIN;
	i8 = getnameinfo((struct sockaddr*)&s4, sizeof(s4), rh4, sizeof(rh4), rs4, sizeof(rs4), flags);
	SANDBOX_END;
	CU_ASSERT_EQUAL(stats.getnameinfo.called, 5);
	CU_ASSERT_EQUAL(custom_gni2_stats.called, 1);
	set_getnameinfo_method(custom_gn1);
	SANDBOX_BEGIN;
	i1 = getnameinfo((struct sockaddr*)&s1, sizeof(s1), rh1, sizeof(rh1), rs1, sizeof(rs1), flags);
	SANDBOX_END;
	CU_ASSERT_EQUAL(stats.getnameinfo.called, 6);
	CU_ASSERT_EQUAL(custom_gni2_stats.called, 1);
	CU_ASSERT_EQUAL(custom_gni1_stats.called, 3);
	// TODO should also test that it doesn't cause any other problem

	// Testing gai_strerror
	CU_ASSERT_EQUAL(monitored.gai_strerror, false);
	CU_ASSERT_EQUAL(stats.gai_strerror.called, 0);
	const char *r1, *r2, *r3, *r4, *r5;
	r1 = gai_strerror(EAI_MEMORY);
	CU_ASSERT_EQUAL(stats.gai_strerror.called, 0);
	SANDBOX_BEGIN;
	r2 = gai_strerror(EAI_MEMORY);
	SANDBOX_END;
	CU_ASSERT_EQUAL(stats.gai_strerror.called, 0);
	CU_ASSERT_EQUAL(strcmp(r1, r2), 0);
	monitored.gai_strerror = true;
	r3 = gai_strerror(EAI_MEMORY);
	CU_ASSERT_EQUAL(stats.gai_strerror.called, 0);
	CU_ASSERT_EQUAL(strcmp(r1, r3), 0);
	SANDBOX_BEGIN;
	r4 = gai_strerror(EAI_MEMORY);
	SANDBOX_END;
	CU_ASSERT_EQUAL(stats.gai_strerror.called, 1);
	CU_ASSERT_EQUAL(stats.gai_strerror.last_params, EAI_MEMORY);
	CU_ASSERT_EQUAL(strcmp(r1, r4), 0);
	CU_ASSERT_EQUAL(custom_gse1_stats.called, 0);
	CU_ASSERT_EQUAL(custom_gse2_stats.called, 0);
	set_gai_strerror_method(custom_gai_strerror1);
	r5 = gai_strerror(EAI_MEMORY);
	CU_ASSERT_EQUAL(strcmp(r1, r5), 0); // Actually, they are strictly identical...
	SANDBOX_BEGIN;
	r2 = gai_strerror(EAI_MEMORY);
	SANDBOX_END;
	CU_ASSERT_EQUAL(stats.gai_strerror.called, 2);
	CU_ASSERT_EQUAL(custom_gse1_stats.called, 1);
	CU_ASSERT_EQUAL(strcmp(r2, "GSE1"), 0);
	set_gai_strerror_method(NULL);
	SANDBOX_BEGIN;
	r3 = gai_strerror(EAI_MEMORY);
	SANDBOX_END;
	CU_ASSERT_EQUAL(strcmp(r3, r1), 0);
	CU_ASSERT_EQUAL(custom_gse1_stats.called, 1);
	set_gai_strerror_method(custom_gai_strerror2);
	SANDBOX_BEGIN;
	r4 = gai_strerror(EAI_MEMORY);
	SANDBOX_END;
	CU_ASSERT_EQUAL(custom_gse2_stats.called, 1);
	CU_ASSERT_EQUAL(strcmp(r4, "GSE2"), 0);
	set_gai_strerror_method(custom_gai_strerror1);
	SANDBOX_BEGIN;
	r5 = gai_strerror(EAI_MEMORY);
	SANDBOX_END;
	CU_ASSERT_EQUAL(custom_gse1_stats.called, 2);
	CU_ASSERT_EQUAL(strcmp(r5, "GSE1"), 0);
	monitored.gai_strerror = false;
	SANDBOX_BEGIN;
	r2 = gai_strerror(EAI_MEMORY);
	SANDBOX_END;
	CU_ASSERT_EQUAL(custom_gse1_stats.called, 2);
	CU_ASSERT_EQUAL(strcmp(r2, r1), 0);
}

void test_checked_freeaddrinfo()
{
	set_test_metadata("checked_freeaddrinfo", _("Tests the checked freeaddrinfo"), 1);
	reinit_all_monitored();
	reinit_all_stats();
	set_check_freeaddrinfo(false);
	reset_gai_fai_gstr_gni_methods();
	memset(&custom_gai1_stats, 0, sizeof(custom_gai1_stats));
	memset(&custom_gai2_stats, 0, sizeof(custom_gai2_stats));
	memset(&custom_fai1_stats, 0, sizeof(custom_fai1_stats));
	memset(&custom_fai2_stats, 0, sizeof(custom_fai2_stats));
	memset(&custom_gni1_stats, 0, sizeof(custom_gni1_stats));
	memset(&custom_gni2_stats, 0, sizeof(custom_gni2_stats));
	memset(&custom_fair_stats, 0, sizeof(custom_fair_stats));
	memset(&custom_gse1_stats, 0, sizeof(custom_gse1_stats));
	memset(&custom_gse2_stats, 0, sizeof(custom_gse2_stats));

	set_freeaddrinfo_badarg_report(custom_checked_fai_reporter);
	set_gai_methods(custom_gai1, custom_fai1);
	set_gai_strerror_method(custom_gai_strerror1);
	// First, let's check that it is not called if we don't activate the check,
	// or if we don't activate monitoring for freeaddrinfo
	// TODO check that its results are consistent only if we
	int i1=-1, i2=-1, i3=-1, i4=-1;
	struct addrinfo hints1, *res1, hints2, *res2, *res3, *res4, *res5, *res6, *res7, *res8;
	memset(&hints1, 0, sizeof(hints1));
	memset(&hints2, 0, sizeof(hints2));
	hints2.ai_family = AF_INET;
	hints2.ai_flags = AI_NUMERICHOST;
	hints2.ai_socktype = SOCK_DGRAM;
	monitored.freeaddrinfo = monitored.getaddrinfo = true;
	SANDBOX_BEGIN;
	i1 = getaddrinfo("127.0.0.1", "80", &hints1, &res1);
	SANDBOX_END;
	CU_ASSERT_EQUAL(custom_gai1_stats.called, 1);
	CU_ASSERT_EQUAL(stats.getaddrinfo.called, 1);
	CU_ASSERT_PTR_NULL(stats.getaddrinfo.addrinfo_list);
	CU_ASSERT_EQUAL(i1, 0);
	CU_ASSERT_EQUAL(res1->ai_family, AF_INET);
	CU_ASSERT_EQUAL(custom_gai1("127.0.0.2", "80", &hints1, &res2), 0); // This shouldn't hurt
	CU_ASSERT_EQUAL(custom_gai1_stats.called, 2);
	(void)i4;
	(void)res7;
	(void)res8;
	SANDBOX_BEGIN;
	freeaddrinfo(res2); // Never seen before, so should call the correct method, but will not
	SANDBOX_END;
	CU_ASSERT_EQUAL(custom_fai1_stats.called, 1); // Called
	CU_ASSERT_EQUAL(custom_fair_stats.called, 0); // Not called
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 1);
	monitored.freeaddrinfo = false;
	CU_ASSERT_EQUAL(getaddrinfo("127.0.0.3", "80", &hints2, &res3), 0); // Use the real one
	set_check_freeaddrinfo(true);
	SANDBOX_BEGIN;
	freeaddrinfo(res3);
	SANDBOX_END;
	CU_ASSERT_EQUAL(custom_fai1_stats.called, 1); // Custom not called
	CU_ASSERT_EQUAL(custom_fair_stats.called, 0);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 1);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.status, 0);
	// Now, let's see what happens if we activate it.
	set_check_freeaddrinfo(true);
	monitored.getaddrinfo = monitored.freeaddrinfo = true;
	SANDBOX_BEGIN;
	i2 = getaddrinfo("127.0.0.4", "80", &hints1, &res4);
	SANDBOX_END;
	CU_ASSERT_EQUAL(custom_gai1_stats.called, 3);
	CU_ASSERT_EQUAL(stats.getaddrinfo.called, 2);
	CU_ASSERT_PTR_NOT_NULL(stats.getaddrinfo.addrinfo_list);
	if (stats.getaddrinfo.addrinfo_list) {
		struct addrinfo_node_t *list = stats.getaddrinfo.addrinfo_list;
		CU_ASSERT_PTR_EQUAL(list->addr_list, res4);
		CU_ASSERT_PTR_NULL(list->next);
	}
	CU_ASSERT_EQUAL(i2, 0);
	CU_ASSERT_EQUAL(custom_gai1("127.0.0.5", "80", &hints1, &res5), 0);
	SANDBOX_BEGIN;
	freeaddrinfo(res4);
	SANDBOX_END;
	CU_ASSERT_EQUAL(custom_fair_stats.called, 0); // Not called because OK
	CU_ASSERT_EQUAL(custom_fai1_stats.called, 2); // Called
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 2);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.status, 0);
	CU_ASSERT_PTR_NULL(stats.getaddrinfo.addrinfo_list);
	SANDBOX_BEGIN;
	freeaddrinfo(res5);
	SANDBOX_END;
	CU_ASSERT_EQUAL(custom_fair_stats.called, 1); // Called because fail
	CU_ASSERT_EQUAL(custom_fai1_stats.called, 3); // Called
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 3);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.status, 1);
	custom_fai1_prevent_free(res4);
	SANDBOX_BEGIN;
	freeaddrinfo(res4); // Double free
	SANDBOX_END;
	CU_ASSERT_EQUAL(custom_fair_stats.called, 2); // Called because fail
	CU_ASSERT_EQUAL(custom_fai1_stats.called, 4); // Called
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 4);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.status, 1);
	// Disabling getaddrinfo should also cause problems...
	// Warning, we need to set gais to default as gai will be library
	set_gai_methods(NULL, NULL);
	monitored.getaddrinfo = false;
	SANDBOX_BEGIN;
	i3 = getaddrinfo("127.0.0.6", "80", &hints2, &res6);
	SANDBOX_END;
	CU_ASSERT_EQUAL(i3, 0);
	SANDBOX_BEGIN;
	freeaddrinfo(res6);
	SANDBOX_END;
	CU_ASSERT_EQUAL(custom_fair_stats.called, 3);
	CU_ASSERT_EQUAL(custom_fai1_stats.called, 4);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 5);
	set_check_freeaddrinfo(true);
	SANDBOX_BEGIN;
	freeaddrinfo(res1); // Should still disappear, but with error
	SANDBOX_END;
	CU_ASSERT_EQUAL(custom_fair_stats.called, 4);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 6);
	CU_ASSERT_EQUAL(custom_fai1_stats.called, 4);
	CU_ASSERT_EQUAL(custom_gai1("127.0.0.7", "80", &hints1, &res7), 0);
	CU_ASSERT_PTR_NULL(stats.getaddrinfo.addrinfo_list);
	set_gai_methods(NULL, custom_fai1);
	set_freeaddrinfo_badarg_report(NULL); // Disable it
	SANDBOX_BEGIN;
	freeaddrinfo(res7);
	SANDBOX_END;
	CU_ASSERT_EQUAL(custom_fai1_stats.called, 5);
	CU_ASSERT_EQUAL(stats.freeaddrinfo.called, 7);
	CU_ASSERT_EQUAL(custom_fair_stats.called, 4); // Disabled, so not called
}

void test_simple_getaddrinfo()
{
	set_test_metadata("simple_gai_fai", _("Tests simple_gai and simple_fai"), 1);
	reinit_all_monitored();
	reinit_all_stats();
	set_check_freeaddrinfo(false);
	reset_gai_fai_gstr_gni_methods();
	set_gai_methods(simple_getaddrinfo, simple_freeaddrinfo);

	int i1=1, i2=1, i3=1, i4=1, i5=1, i6=1, i7=1, i8=1, i9=1;
	int i10=0, i11=0, i12=0, i13=0, i14=0, i15=0;
	struct addrinfo hints1, *res1, hints2, *res2, hints3, *res3, hints4, *res4;
	struct addrinfo hints5, *res5, hints6, *res6, hints7, *res7, hints8, *res8, *res9;
	struct addrinfo hints10, *res10=NULL, hints11, *res11=NULL, hints12, *res12=NULL;
	struct addrinfo hints13, *res13=NULL, hints14, *res14=NULL, hints15, *res15=NULL;
	memset(&hints1, 0, sizeof(hints1));
	memset(&hints2, 0, sizeof(hints2));
	memset(&hints3, 0, sizeof(hints3));
	memset(&hints4, 0, sizeof(hints4));
	memset(&hints5, 0, sizeof(hints5));
	memset(&hints6, 0, sizeof(hints6));
	memset(&hints7, 0, sizeof(hints7));
	memset(&hints8, 0, sizeof(hints8));
	memset(&hints10, 0, sizeof(hints10));

	hints1.ai_flags = AI_NUMERICHOST | AI_CANONNAME;
	hints1.ai_family = AF_INET;
	hints1.ai_socktype = SOCK_STREAM;
	hints2.ai_flags = AI_NUMERICHOST | AI_PASSIVE; // Should ignore the AI_PASSIVE
	hints2.ai_family = AF_INET;
	hints2.ai_socktype = SOCK_DGRAM;
	hints3.ai_flags = AI_NUMERICHOST;
	hints3.ai_family = AF_INET;
	hints3.ai_socktype = SOCK_DGRAM;
	hints4.ai_flags = AI_PASSIVE;
	hints4.ai_family = AF_INET;
	hints4.ai_socktype = SOCK_DGRAM;
	hints5.ai_flags = AI_PASSIVE;
	hints5.ai_family = AF_INET6;
	hints5.ai_socktype = 0;
	hints6.ai_flags = AI_CANONNAME;
	hints6.ai_family = AF_UNSPEC;
	hints6.ai_socktype = SOCK_STREAM;
	hints7.ai_flags = AI_NUMERICHOST;
	hints7.ai_family = AF_INET6;
	hints7.ai_socktype = SOCK_DGRAM;
	hints8.ai_flags = AI_NUMERICHOST;
	hints8.ai_family = AF_INET6;
	hints8.ai_socktype = SOCK_STREAM;

	hints10.ai_flags = AI_NUMERICHOST;
	hints10.ai_family = AF_INET6;
	hints10.ai_socktype = SOCK_DGRAM;
	hints11.ai_flags = AI_NUMERICHOST;
	hints11.ai_family = AF_UNSPEC;
	hints11.ai_socktype = 0;
	hints12.ai_flags = AI_NUMERICHOST;
	hints12.ai_family = AF_UNSPEC;
	hints12.ai_socktype = 0;
	hints13.ai_flags = AI_NUMERICHOST;
	hints13.ai_family = AF_INET;
	hints13.ai_socktype = 0;
	hints14.ai_flags = AI_NUMERICHOST;
	hints14.ai_family = AF_INET;
	hints14.ai_socktype = SOCK_DGRAM;
	hints15.ai_flags = AI_NUMERICHOST | AI_CANONNAME;
	hints15.ai_family = AF_INET;
	hints15.ai_socktype = SOCK_DGRAM;

	monitored.getaddrinfo = true;
	monitored.freeaddrinfo = true;

	SANDBOX_BEGIN;
	i1 = getaddrinfo("127.0.0.1", "80", &hints1, &res1); // ACTIVE, loopback, IPv4, canon name
	i2 = getaddrinfo("192.168.1.1", "443", &hints2, &res2); // ACTIVE, IPv4, false passive
	i3 = getaddrinfo(NULL, "80", &hints3, &res3); // ACTIVE, implicit loopback, IPv4
	i4 = getaddrinfo(NULL, "80", &hints4, &res4); // PASSIVE, IPv4
	i5 = getaddrinfo(NULL, "22", &hints5, &res5); // PASSIVE, IPv6
	i6 = getaddrinfo("::1", "22", &hints6, &res6); // ACTIVE, loopback, IPv6
	i7 = getaddrinfo("::2", NULL, &hints7, &res7); // ACTIVE, IPv6, no port
	i8 = getaddrinfo(NULL, "443", &hints8, &res8); // ACTIVE, implicit loopback, IPv6
	i9 = getaddrinfo("1.2.3.4", "80", NULL, &res9); // ACTIVE, IPv4, no hint
	// Errors
	i10 = getaddrinfo("123.45.67.89", "80", &hints10, &res10); // IPv4 vs IPv6
	i11 = getaddrinfo("www.example.com", "80", &hints11, &res11); // non-numeric host
	i12 = getaddrinfo("::1", "http", &hints12, &res12); // non-numeric serv
	i13 = getaddrinfo("1::1", "80", &hints13, &res13); // IPv6 vs IPv4
	i14 = getaddrinfo(NULL, NULL, &hints14, &res14); // both NULL
	i15 = getaddrinfo(NULL, "80", &hints15, &res15); // canon name without host
	SANDBOX_END;

	CU_ASSERT_EQUAL(i1, 0);
	CU_ASSERT_PTR_NOT_NULL(res1);
	if (res1) {
		CU_ASSERT_EQUAL(res1->ai_family, AF_INET);
		CU_ASSERT_EQUAL(res1->ai_socktype, SOCK_STREAM);
		CU_ASSERT_EQUAL(res1->ai_addrlen, sizeof(struct sockaddr_in));
		struct sockaddr_in *addr = (struct sockaddr_in*)(res1->ai_addr);
		CU_ASSERT_PTR_NOT_NULL(addr);
		if (addr) {
			CU_ASSERT_EQUAL(addr->sin_family, AF_INET);
			CU_ASSERT_EQUAL(addr->sin_port, htons(80));
			struct in_addr ipaddr;
			inet_pton(AF_INET, "127.0.0.1", &ipaddr);
			CU_ASSERT_EQUAL(addr->sin_addr.s_addr, ipaddr.s_addr);
		}
		CU_ASSERT_PTR_NOT_NULL(res1->ai_canonname);
		if (res1->ai_canonname) {
			CU_ASSERT_EQUAL(strcmp(res1->ai_canonname, "C127.0.0.1"), 0);
		}
	}
	CU_ASSERT_EQUAL(i2, 0);
	CU_ASSERT_PTR_NOT_NULL(res2);
	if (res2) {
		CU_ASSERT_EQUAL(res2->ai_family, AF_INET);
		CU_ASSERT_EQUAL(res2->ai_socktype, SOCK_DGRAM);
		CU_ASSERT_EQUAL(res2->ai_addrlen, sizeof(struct sockaddr_in));
		struct sockaddr_in *addr = (struct sockaddr_in*)(res2->ai_addr);
		CU_ASSERT_PTR_NOT_NULL(addr);
		if (addr) {
			CU_ASSERT_EQUAL(addr->sin_family, AF_INET);
			CU_ASSERT_EQUAL(addr->sin_port, htons(443));
			struct in_addr ipaddr;
			inet_pton(AF_INET, "192.168.1.1", &ipaddr);
			CU_ASSERT_EQUAL(addr->sin_addr.s_addr, ipaddr.s_addr);
		}
		CU_ASSERT_PTR_NULL(res2->ai_canonname);
	}
	CU_ASSERT_EQUAL(i3, 0);
	CU_ASSERT_PTR_NOT_NULL(res3);
	if (res3) {
		CU_ASSERT_EQUAL(res3->ai_family, AF_INET);
		CU_ASSERT_EQUAL(res3->ai_socktype, SOCK_DGRAM);
		CU_ASSERT_EQUAL(res3->ai_addrlen, sizeof(struct sockaddr_in));
		struct sockaddr_in *addr = (struct sockaddr_in*)(res3->ai_addr);
		CU_ASSERT_PTR_NOT_NULL(addr);
		if (addr) {
			CU_ASSERT_EQUAL(addr->sin_family, AF_INET);
			CU_ASSERT_EQUAL(addr->sin_port, htons(80));
			struct in_addr ipaddr;
			inet_pton(AF_INET, "127.0.0.1", &ipaddr);
			CU_ASSERT_EQUAL(addr->sin_addr.s_addr, ipaddr.s_addr);
		}
		CU_ASSERT_PTR_NULL(res3->ai_canonname);
	}
	CU_ASSERT_EQUAL(i4, 0);
	CU_ASSERT_PTR_NOT_NULL(res4);
	if (res4) {
		CU_ASSERT_EQUAL(res4->ai_family, AF_INET);
		CU_ASSERT_EQUAL(res4->ai_socktype, SOCK_DGRAM);
		CU_ASSERT_EQUAL(res4->ai_addrlen, sizeof(struct sockaddr_in));
		struct sockaddr_in *addr = (struct sockaddr_in*)(res4->ai_addr);
		CU_ASSERT_PTR_NOT_NULL(addr);
		if (addr) {
			CU_ASSERT_EQUAL(addr->sin_family, AF_INET);
			CU_ASSERT_EQUAL(addr->sin_port, htons(80));
			CU_ASSERT_EQUAL(addr->sin_addr.s_addr, INADDR_ANY);
		}
		CU_ASSERT_PTR_NULL(res4->ai_canonname);
	}
	CU_ASSERT_EQUAL(i5, 0);
	CU_ASSERT_PTR_NOT_NULL(res5);
	if (res5) {
		CU_ASSERT_EQUAL(res5->ai_family, AF_INET6);
		CU_ASSERT_EQUAL(res5->ai_addrlen, sizeof(struct sockaddr_in6));
		struct sockaddr_in6 *addr = (struct sockaddr_in6*)(res5->ai_addr);
		CU_ASSERT_PTR_NOT_NULL(addr);
		if (addr) {
			CU_ASSERT_EQUAL(addr->sin6_family, AF_INET6);
			CU_ASSERT_EQUAL(addr->sin6_port, htons(22));
			CU_ASSERT_EQUAL(memcmp(&(addr->sin6_addr), &(in6addr_any), sizeof(struct in6_addr)), 0);
		}
		CU_ASSERT_PTR_NULL(res5->ai_canonname);
	}
	CU_ASSERT_EQUAL(i6, 0);
	CU_ASSERT_PTR_NOT_NULL(res6);
	if (res6) {
		CU_ASSERT_EQUAL(res6->ai_family, AF_INET6);
		CU_ASSERT_EQUAL(res6->ai_socktype, SOCK_STREAM);
		CU_ASSERT_EQUAL(res6->ai_addrlen, sizeof(struct sockaddr_in6));
		struct sockaddr_in6 *addr = (struct sockaddr_in6*)(res6->ai_addr);
		CU_ASSERT_PTR_NOT_NULL(addr);
		if (addr) {
			CU_ASSERT_EQUAL(addr->sin6_family, AF_INET6);
			CU_ASSERT_EQUAL(addr->sin6_port, htons(22));
			struct in6_addr addr6;
			inet_pton(AF_INET6, "::1", &addr6);
			CU_ASSERT_EQUAL(memcmp(&(addr->sin6_addr), &(addr6), sizeof(addr6)), 0);
		}
		CU_ASSERT_PTR_NOT_NULL(res6->ai_canonname);
		if (res6->ai_canonname) {
			CU_ASSERT_EQUAL(strcmp(res6->ai_canonname, "C::1"), 0);
		}
	}
	CU_ASSERT_EQUAL(i7, 0);
	CU_ASSERT_PTR_NOT_NULL(res7);
	if (res7) {
		CU_ASSERT_EQUAL(res7->ai_family, AF_INET6);
		CU_ASSERT_EQUAL(res7->ai_socktype, SOCK_DGRAM);
		CU_ASSERT_EQUAL(res7->ai_addrlen, sizeof(struct sockaddr_in6));
		struct sockaddr_in6 *addr = (struct sockaddr_in6*)(res7->ai_addr);
		CU_ASSERT_PTR_NOT_NULL(addr);
		if (addr) {
			CU_ASSERT_EQUAL(addr->sin6_family, AF_INET6);
			struct in6_addr addr6;
			inet_pton(AF_INET6, "::2", &addr6);
			CU_ASSERT_EQUAL(memcmp(&(addr->sin6_addr), &(addr6), sizeof(addr6)), 0);
		}
		CU_ASSERT_PTR_NULL(res7->ai_canonname);
	}
	CU_ASSERT_EQUAL(i8, 0);
	CU_ASSERT_PTR_NOT_NULL(res8);
	if (res8) {
		CU_ASSERT_EQUAL(res8->ai_family, AF_INET6);
		CU_ASSERT_EQUAL(res8->ai_socktype, SOCK_STREAM);
		CU_ASSERT_EQUAL(res8->ai_addrlen, sizeof(struct sockaddr_in6));
		struct sockaddr_in6 *addr = (struct sockaddr_in6*)(res8->ai_addr);
		CU_ASSERT_PTR_NOT_NULL(addr);
		if (addr) {
			CU_ASSERT_EQUAL(addr->sin6_family, AF_INET6);
			CU_ASSERT_EQUAL(addr->sin6_port, htons(443));
			struct in6_addr addr6;
			inet_pton(AF_INET6, "::1", &addr6);
			CU_ASSERT_EQUAL(memcmp(&(addr->sin6_addr), &(addr6), sizeof(addr6)), 0);
		}
		CU_ASSERT_PTR_NULL(res8->ai_canonname);
	}
	CU_ASSERT_EQUAL(i9, 0);
	CU_ASSERT_PTR_NOT_NULL(res9);
	if (res9) {
		CU_ASSERT_EQUAL(res9->ai_family, AF_INET);
		CU_ASSERT_EQUAL(res9->ai_socktype, SOCK_DGRAM);
		CU_ASSERT_EQUAL(res9->ai_addrlen, sizeof(struct sockaddr_in));
		struct sockaddr_in *addr = (struct sockaddr_in*)(res9->ai_addr);
		CU_ASSERT_PTR_NOT_NULL(addr);
		if (addr) {
			CU_ASSERT_EQUAL(addr->sin_family, AF_INET);
			CU_ASSERT_EQUAL(addr->sin_port, htons(80));
			struct in_addr addr4;
			inet_pton(AF_INET, "1.2.3.4", &addr4);
			CU_ASSERT_EQUAL(addr->sin_addr.s_addr, addr4.s_addr);
		}
		CU_ASSERT_PTR_NULL(res9->ai_canonname);
	}
	// The failed ones
	CU_ASSERT_EQUAL(i10, EAI_FAMILY);
	CU_ASSERT_EQUAL(i11, EAI_NONAME);
	CU_ASSERT_EQUAL(i12, EAI_NONAME);
	CU_ASSERT_EQUAL(i13, EAI_FAMILY);
	CU_ASSERT_EQUAL(i14, EAI_NONAME);
	CU_ASSERT_EQUAL(i15, EAI_BADFLAGS);

	SANDBOX_BEGIN;
	// Check freeaddrinfo
	freeaddrinfo(res1);
	freeaddrinfo(res2);
	freeaddrinfo(res3);
	freeaddrinfo(res4);
	freeaddrinfo(res5);
	freeaddrinfo(res6);
	freeaddrinfo(res7);
	freeaddrinfo(res8);
	freeaddrinfo(res9);
	// The following should still work
	freeaddrinfo(res10);
	freeaddrinfo(res11);
	freeaddrinfo(res12);
	freeaddrinfo(res13);
	freeaddrinfo(res14);
	SANDBOX_END;
}

int main(int argc, char **argv)
{
	RUN(test_gai_substitutes, test_checked_freeaddrinfo, test_simple_getaddrinfo);
}

