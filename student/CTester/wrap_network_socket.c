/*
 * Wrapper for accept, bind, connect, listen, poll, select,
 * recv, recvfrom, recvmsg, send, sendto, sendmsg, socket.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <poll.h>

#include "wrap.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define BILLION (1000*1000*1000)


int     __real_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int     __real_bind   (int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int     __real_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int     __real_listen(int sockfd, int backlog);
int     __real_poll(struct pollfd *fds, nfds_t nfds, int timeout);
ssize_t __real_recv(int sockfd, void *buf, size_t len, int flags);
ssize_t __real_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t __real_recvmsg(int sockfd, struct msghdr *msg, int flags);
int     __real_select(int nfds, fd_set *readfds, fd_set *write_fds, fd_set *except_fds, struct timeval *timeout);
ssize_t __real_send(int sockfd, const void *buf, size_t len, int flags);
ssize_t __real_sendmsg(int sockfd, const struct msghdr *msg, int flags);
ssize_t __real_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
int     __real_shutdown(int sockfd, int how);
int     __real_socket(int domain, int type, int protocol);


extern bool wrap_monitoring;
extern struct wrap_stats_t stats;
extern struct wrap_monitor_t monitored;
extern struct wrap_fail_t failures;


/**
 * Auxiliary structures and functions
 */

void getnanotime(struct timespec *res)
{
    clock_gettime(CLOCK_REALTIME, res);
}

int64_t get_time_interval(const struct timespec *pasttime, const struct timespec *curtime)
{
    int64_t past = pasttime->tv_sec;
    past *= 1000*1000*1000;
    past += pasttime->tv_nsec;
    int64_t cur = curtime->tv_sec;
    cur *= 1000*1000*1000;
    cur += curtime->tv_nsec;
    int64_t interval = cur - past;
    if (interval < 0) {
        fprintf(stderr, "BUG: negative time interval");
    }
    return interval;
}


/**
 * recv-related
 */

// #define RECV_MODE_BUFCHUK 1 // not used

/**
 * Precision for the 'interval' field.
 * It contains the wait time for the current chunk, as measured from the previous call.
 * It may be positive: in this case, it means the next data was not available
 * at the end of the previous call, and it is the time the current call has
 * to wait before accessing data. Minus the interval between the two calls.
 * It may be negative: in this case, it represents the opposite of the amout
 * of time the current chunk has been available.
 * Thanks to this field, we can enable the chunks at more or less the right moment.
 */
struct recv_item {
    int fd; // the file descriptor this structure applies to
    //int mode; // the type of data provider used; not used
    const struct recv_buffer_t *buf; // Provided recv_buffer_t structure
    unsigned int chunk_id; // Current chunk, or next chunk to be received
    size_t bytes_read; // Number of bytes read inside the current chunk
    struct timespec last_time; // Time of the end of the last call of recv on this socket
    int64_t interval; // In real-time mode (RECV_REAL_INTERVAL), real wait interval for the current chunk.
};

struct recv_fd_table_t {
    size_t n;
    struct recv_item *items;
} recv_fd_table;

struct recv_item *recv_get_entry(int fd)
{
    for (unsigned int i = 0; i < recv_fd_table.n; i++) {
        if (recv_fd_table.items[i].fd == fd)
            return &(recv_fd_table.items[i]);
    }
    return NULL;
}

bool fd_is_recv_buffered(int fd)
{
    return (recv_get_entry(fd) != NULL);
}

void reinit_recv_fd_table_item(int i)
{
    // We need to clean up _recv_fd_table.items[i]
    recv_fd_table.items[i].buf = NULL; // We're not responsible to free it.
    recv_fd_table.items[i].chunk_id = 0;
    recv_fd_table.items[i].interval = 0;
    recv_fd_table.items[i].last_time = (struct timespec) {
        .tv_sec = 0,
        .tv_nsec = 0
    };
}

void reinit_recv_fd_table()
{
    // As we're not responsible to clean up all the recv_buffer_t, we can just free everything up
    free(recv_fd_table.items);
    recv_fd_table.n = 0;
}

int recv_remove_entry(int fd)
{
    bool found = false;
    for (unsigned int i = 0; i < recv_fd_table.n; i++) {
        if (recv_fd_table.items[i].fd == fd) {
            found = true;
            if (i < recv_fd_table.n - 1) {
                struct recv_item *dest = &(recv_fd_table.items[i]);
                memmove(dest, dest + 1, recv_fd_table.n - i - 1);
                memset(&(recv_fd_table.items[recv_fd_table.n-1]), 0, sizeof(struct recv_item));
                // This is not really a problem that we have a useless thing at the end
            }
            recv_fd_table.n--;
        }
    }
    return (found ? 1 : 0);
}

/**
 * Returns a pointer to an entry in the table,
 * where we can safely store our informations.
 * May be a pre-existing chunk,
 * in which case the fd's will match.
 */
struct recv_item *recv_get_new_entry(int fd)
{
    unsigned int i = 0;
    for (i = 0; i < recv_fd_table.n; i++) {
        if (recv_fd_table.items[i].fd == fd) {
            // We should clean it before changing it
            reinit_recv_fd_table_item(i);
            break;
        }
    }
    if (i >= recv_fd_table.n) {
        // realloc
        struct recv_item *tmp = realloc(recv_fd_table.items, (recv_fd_table.n + 1) * sizeof(struct recv_item));
        if (tmp == NULL)
            return NULL;
        recv_fd_table.items = tmp;
        recv_fd_table.n++;
    }
    // Now, we can insert at index i
    return &(recv_fd_table.items[i]);
}

size_t recv_handle_buffer_no_rt(struct recv_item *cur, void *buf, size_t len, int flags, int64_t call_interval)
{
    const struct recv_bufchunk_t *curchunk = &(cur->buf->chunks[cur->chunk_id]);
    if (cur->bytes_read == 0) {
        // We may have to wait
        int64_t sleeptime = 0;
        if (cur->buf->mode == RECV_BEFORE_INTERVAL) {
            sleeptime = curchunk->interval;
        } else {
            sleeptime = curchunk->interval - call_interval;
        }
        if (sleeptime > 0) {
            if ((flags & MSG_DONTWAIT) != 0) {
                errno = EAGAIN;
                return -1;
            }
            struct timespec tmp = (struct timespec) {
                .tv_sec = sleeptime / BILLION,
                .tv_nsec = sleeptime % BILLION
            };
            nanosleep(&tmp, NULL);
            getnanotime(&(cur->last_time)); // We need to update
        }
    }
    size_t bytes_left = curchunk->buflen - cur->bytes_read;
    size_t transfered_bytes = MIN(len, bytes_left);
    memmove(buf, (curchunk->buf + cur->bytes_read), transfered_bytes);
    cur->bytes_read += transfered_bytes;
    if (cur->bytes_read >= curchunk->buflen) {
        cur->chunk_id++;
        cur->bytes_read = 0;
    }
    return transfered_bytes;
}

size_t recv_handle_buffer_rt(struct recv_item *cur, void *buf, size_t len, int flags, int64_t call_interval)
{
    /*
     * Assume the following:
     * - cur->interval was the time to wait for the current chunk to become active, at the end of the previous call.
     * - cur->bytes_read is 0 only if we have to wait.
     */
    cur->interval -= call_interval;
    /*
     * Assume the following:
     * - cur->interval is the remaining time to wait for the current chunk to become active.
     * - cur->bytes_read is 0 only if we have to wait.
     */
    if (cur->interval > 0) {
        if ((flags & MSG_DONTWAIT) != 0) {
            // The call would block but the caller requested it shouldn't block
            errno = EAGAIN;
            return -1;
        }
        int64_t sleeptime = cur->interval;
        struct timespec tmp = (struct timespec) {
            .tv_sec = sleeptime / BILLION,
            .tv_nsec = sleeptime % BILLION
        };
        nanosleep(&tmp, NULL);
        cur->interval = 0;
        getnanotime(&(cur->last_time));
    }
    /*
     * Assume the following:
     * - cur->interval is the negative of the time the current chunk has been available.
     */
    size_t total_bytes = 0;
    for (unsigned int i = cur->chunk_id; i < cur->buf->nchunks; i++) {
        if (len <= 0) {
            break;
        }
        const struct recv_bufchunk_t *curchunk = &(cur->buf->chunks[i]);
        size_t bytes_left = curchunk->buflen - cur->bytes_read;
        size_t transfered_bytes = MIN(len, bytes_left);
        memmove((buf + total_bytes), (curchunk->buf + cur->bytes_read), transfered_bytes);
        cur->bytes_read += transfered_bytes;
        len -= transfered_bytes;
        total_bytes += transfered_bytes;
        if (cur->bytes_read >= curchunk->buflen) {
            // Emptied chunk
            cur->chunk_id++;
            cur->bytes_read = 0;
            if (cur->chunk_id < cur->buf->nchunks) {
                // Update interval
                cur->interval += cur->buf->chunks[cur->chunk_id].interval;
                if (cur->interval > 0) {
                    break; // Not available yet
                }
            }
        }
    }
    return total_bytes;
}

ssize_t recv_handle_buffer(int sockfd, void *buf, size_t len, int flags)
{
    struct recv_item *cur = recv_get_entry(sockfd);
    if (cur == NULL) {
        errno = EINTR; // This is about the only case where this can happen
        return -1;
    }
    struct timespec curtime;
    getnanotime(&curtime);
    int64_t call_interval = get_time_interval(&(cur->last_time), &curtime);
    cur->last_time = curtime;
    if (cur->chunk_id >= cur->buf->nchunks) {
        // Nothing left to read; we can return immediately 0
        // TODO remove this as a particular case
        return 0;
    }
    size_t bytes_transfered = 0;
    if (cur->buf->mode == RECV_BEFORE_INTERVAL || cur->buf->mode == RECV_AFTER_INTERVAL) {
        bytes_transfered = recv_handle_buffer_no_rt(cur, buf, len, flags, call_interval);
    } else if (cur->buf->mode == RECV_REAL_INTERVAL) {
        bytes_transfered = recv_handle_buffer_rt(cur, buf, len, flags, call_interval);
    } else {
        return -1;
    }
    return bytes_transfered;
}

/**
 * Wrap functions.
 */


/*
 * Note: addr should point to an existing sockaddr, or NULL if we don't want it
 * and addrlen should point to an existing socklen_t containing the size
 * of the existing sockaddr, and will be modified to reflect the true size
 * of the returned sockaddr; it may be greater than the original value
 * if the provided sockaddr is too small, and this sockaddr will be truncated.
 * If addr in NULL, addrlen should also be NULL.
 */
int __wrap_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    if (!(wrap_monitoring && monitored.accept)) {
        return __real_accept(sockfd, addr, addrlen);
    }
    socklen_t old_addrlen = (addrlen == NULL ? 0 : *addrlen); // May crash if addrlen doesn't point to a valid address.
    stats.accept.called++;
    stats.accept.last_params = (struct params_accept_t) {
        .sockfd = sockfd,
        .addr = addr,
        .addrlen_ptr = addrlen
    };
    // Reinit stats
    stats.accept.last_returns.addrlen = 0;
    memset(&(stats.accept.last_returns.addr), 0, sizeof(struct sockaddr_storage));
    if (FAIL(failures.accept)) {
        failures.accept = NEXT(failures.accept);
        errno = failures.accept_errno;
        return (stats.accept.last_return = failures.accept_ret);
    }
    failures.accept = NEXT(failures.accept);
    int ret = -2;
    ret = __real_accept(sockfd, addr, addrlen);
    if (ret == 0 && addr != NULL) {
        /*
         * We should only copy the returned address
         * - if there was no error (otherwise it is probably not wise)
         * - if addr != NULL; if addr == NULL, then whatever addrlen is,
         *   nothing was returned
         */
        stats.accept.last_returns.addrlen = (addrlen == NULL ? 0 : *addrlen);
        /*
         * We need the minimum as *addrlen may be greater than old_addrlen
         * and grater than the size of *addr, and we're not interested
         * in potential garbage not set by accept in the remaining space.
         * This can't segfault as ret == 0
         */
        memcpy(&(stats.accept.last_returns.addr), addr, MIN(old_addrlen, *addrlen));
    } // else: there was an error, so the safest thing to do is ignore the value provided
    return (stats.accept.last_return = ret);
}

int __wrap_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    if (!(wrap_monitoring && monitored.bind)) {
        return __real_bind(sockfd, addr, addrlen);
    }
    stats.bind.called++;
    stats.bind.last_params = (struct params_bind_t) {
        .sockfd = sockfd,
        .addr = addr,
        .addrlen = addrlen
    };
    if (FAIL(failures.bind)) {
        failures.bind = NEXT(failures.bind);
        errno = failures.bind_errno;
        return (stats.bind.last_return = failures.bind_ret);
    }
    failures.bind = NEXT(failures.bind);
    int ret = -2;
    ret = __real_bind(sockfd, addr, addrlen);
    return (stats.bind.last_return = ret);
}

int __wrap_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    if (!(wrap_monitoring && monitored.connect)) {
        return __real_connect(sockfd, addr, addrlen);
    }
    stats.connect.called++;
    stats.connect.last_params = (struct params_connect_t) {
        .sockfd = sockfd,
        .addr = addr,
        .addrlen = addrlen
    };
    if (FAIL(failures.connect)) {
        failures.connect = NEXT(failures.connect);
        errno = failures.connect_errno;
        return (stats.connect.last_return = failures.connect_ret);
    }
    failures.connect = NEXT(failures.connect);
    int ret = -2;
    ret = __real_connect(sockfd, addr, addrlen);
    return (stats.connect.last_return = ret);
}

int __wrap_listen(int sockfd, int backlog)
{
    if (!(wrap_monitoring && monitored.listen)) {
        return __real_listen(sockfd, backlog);
    }
    stats.listen.called++;
    stats.listen.last_params = (struct params_listen_t) {
        .sockfd = sockfd,
        .backlog = backlog
    };
    if (FAIL(failures.listen)) {
        failures.listen = NEXT(failures.listen);
        errno = failures.listen_errno;
        return (stats.listen.last_return = failures.listen_ret);
    }
    failures.listen = NEXT(failures.listen);
    int ret = -2;
    ret = __real_listen(sockfd, backlog);
    return (stats.listen.last_return = ret);
}

int __wrap_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    if (!(wrap_monitoring && monitored.poll)) {
        return __real_poll(fds, nfds, timeout);
    }
    stats.poll.called++;
    stats.poll.last_params = (struct params_poll_t) {
        .fds_ptr = fds,
        .nfds = nfds,
        .timeout = timeout,
        .fds_copy = NULL
    };
    if (FAIL(failures.poll)) {
        failures.poll = NEXT(failures.poll);
        errno = failures.poll_errno;
        return (stats.poll.last_return = failures.poll_ret);
    }
    failures.poll = NEXT(failures.poll);
    int ret = -2;
    ret = __real_poll(fds, nfds, timeout);
    if (! (ret == -1 && errno == EFAULT)) {
        struct pollfd *tmp = malloc(nfds * sizeof(struct pollfd));
        if (tmp) {
            memcpy(fds, tmp, nfds * sizeof(struct pollfd));
            stats.poll.last_params.fds_copy = tmp;
        }
    }
    return ret;
}

ssize_t __wrap_recv(int sockfd, void *buf, size_t len, int flags)
{
    if (!(wrap_monitoring && monitored.recv)) {
        return __real_recv(sockfd, buf, len, flags);
    }
    stats.recv.called++;
    stats.recv_all.called++;
    stats.recv.last_params = (struct params_recv_t) {
        .sockfd = sockfd,
        .buf = buf,
        .len = len,
        .flags = flags
    };
    if (FAIL(failures.recv)) {
        failures.recv = NEXT(failures.recv);
        errno = failures.recv_errno;
        return (stats.recv.last_return = failures.recv_ret);
    }
    failures.recv = NEXT(failures.recv);
    ssize_t ret = -1;
    if (fd_is_recv_buffered(sockfd)) {
        ret = recv_handle_buffer(sockfd, buf, len, flags);
    } else {
        ret = __real_recv(sockfd, buf, len, flags);
    }
    return (stats.recv.last_return = ret);
}

ssize_t __wrap_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
    if (!(wrap_monitoring && monitored.recvfrom)) {
        return __real_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
    }
    socklen_t old_addrlen = (addrlen == NULL ? 0 : *addrlen); // May crash
    stats.recvfrom.called++;
    stats.recv_all.called++;
    stats.recvfrom.last_params = (struct params_recvfrom_t) {
        .sockfd = sockfd,
        .buf = buf,
        .len = len,
        .flags = flags,
        .src_addr = src_addr,
        .addrlen_ptr = addrlen
    };
    stats.recvfrom.last_returned_addr.addrlen = 0;
    memset(&(stats.recvfrom.last_returned_addr.src_addr), 0, sizeof(struct sockaddr_storage));
    if (FAIL(failures.recvfrom)) {
        failures.recvfrom = NEXT(failures.recvfrom);
        errno = failures.recvfrom_errno;
        return (stats.recv.last_return = failures.recvfrom_ret);
    }
    failures.recvfrom = NEXT(failures.recvfrom);
    ssize_t ret = -1;
    ret = __real_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
    if (ret >= 0 && src_addr != NULL) {
        // Same justification as in accept
        stats.recvfrom.last_returned_addr.addrlen = *addrlen;
        memcpy(&(stats.recvfrom.last_returned_addr.src_addr), src_addr, MIN(old_addrlen, *addrlen));
    }
    return (stats.recvfrom.last_return = ret);
}

ssize_t __wrap_recvmsg(int sockfd, struct msghdr *msg, int flags)
{
    if (!(wrap_monitoring && monitored.recvmsg)) {
        return __real_recvmsg(sockfd, msg, flags);
    }
    stats.recvmsg.called++;
    stats.recv_all.called++;
    stats.recvmsg.last_params = (struct params_recvmsg_t) {
        .sockfd = sockfd,
        .msg = msg,
        .flags = flags
    };
    // Reinit struct
    memset(&(stats.recvmsg.last_returned_msg), 0, sizeof(struct msghdr));
    if (FAIL(failures.recvmsg)) {
        failures.recvmsg = NEXT(failures.recvmsg);
        errno = failures.recvmsg_errno;
        return (stats.recvmsg.last_return = failures.recvmsg_ret);
    }
    failures.recvmsg = NEXT(failures.recvmsg);
    ssize_t ret = -1;
    ret = __real_recvmsg(sockfd, msg, flags);
    if (ret == 0) {
        // Assume that msg doesn't point to an invalid location
        memcpy(&(stats.recvmsg.last_returned_msg), msg, sizeof(struct msghdr));
    }
    return ret;
}

int __wrap_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    if (!(wrap_monitoring && monitored.select)) {
        return __real_select(nfds, readfds, writefds, exceptfds, timeout);
    }
    stats.select.called++;
    stats.select.last_params = (struct params_select_t) {
        .nfds = nfds,
        .readfds_ptr = readfds,
        .writefds_ptr = writefds,
        .exceptfds_ptr = exceptfds,
        .timeout_ptr = timeout,
        .readfds = *readfds, // FIXME may cause a segfault if the student passed garbage
        .writefds = *writefds,
        .exceptfds = *exceptfds,
        .timeout = *timeout
    };
    if (FAIL(failures.select)) {
        failures.select = NEXT(failures.select);
        errno = failures.select_errno;
        return (stats.select.last_return = failures.select_ret);
    }
    failures.select = NEXT(failures.select);
    int ret = -1;
    ret = __real_select(nfds, readfds, writefds, exceptfds, timeout);
    return ret;
}

ssize_t __wrap_send(int sockfd, const void *buf, size_t len, int flags)
{
    if (!(wrap_monitoring && monitored.send)) {
        return __real_send(sockfd, buf, len, flags);
    }
    stats.send.called++;
    stats.send_all.called++;
    stats.send.last_params = (struct params_send_t) {
        .sockfd = sockfd,
        .buf = buf,
        .len = len,
        .flags = flags
    };
    if (FAIL(failures.send)) {
        failures.send = NEXT(failures.send);
        errno = failures.send_errno;
        return (stats.send.last_return = failures.send_ret);
    }
    failures.send = NEXT(failures.send);
    ssize_t ret = -1;
    ret = __real_send(sockfd, buf, len, flags);
    return (stats.send.last_return = ret);
}

ssize_t __wrap_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
    if (!(wrap_monitoring && monitored.sendto)) {
        return __real_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
    }
    stats.sendto.called++;
    stats.send_all.called++;
    stats.sendto.last_params = (struct params_sendto_t) {
        .sockfd = sockfd,
        .buf = buf,
        .len = len,
        .flags = flags,
        .dest_addr_ptr = dest_addr,
        //.dest_addr = (struct sockaddr_storage)0,
        .addrlen = addrlen
    };
    /*if (dest_addr != NULL) {
        memcpy(&(stats.sendto.last_params.dest_addr), dest_addr, MIN(addrlen, sizeof(struct sockaddr_storage))); // May segfault if dest_addr is badly defined.
    }*/
    if (FAIL(failures.sendto)) {
        failures.sendto = NEXT(failures.sendto);
        errno = failures.sendto_errno;
        return (stats.sendto.last_return = failures.sendto_ret);
    }
    failures.sendto = NEXT(failures.sendto);
    ssize_t ret = -1;
    ret = __real_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
    return (stats.sendto.last_return = ret);
}

ssize_t __wrap_sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
    if (!(wrap_monitoring && monitored.sendmsg)) {
        return __real_sendmsg(sockfd, msg, flags);
    }
    stats.sendmsg.called++;
    stats.send_all.called++;
    stats.sendmsg.last_params = (struct params_sendmsg_t) {
        .sockfd = sockfd,
        .msg_ptr = msg,
        //.msg = (struct msghdr)0,
        .flags = flags
    };
    /*if (msg != NULL) {
        memcpy(&(stats.sendmsg.last_params.msg), msg, sizeof(struct msghdr));
    }*/
    if (FAIL(failures.sendmsg)) {
        failures.sendmsg = NEXT(failures.sendmsg);
        errno = failures.sendmsg_errno;
        return (stats.sendmsg.last_return = failures.sendmsg_ret);
    }
    failures.sendmsg = NEXT(failures.sendmsg);
    ssize_t ret = -1;
    ret = __real_sendmsg(sockfd, msg, flags);
    return ret;
}

int __wrap_shutdown(int sockfd, int how)
{
    if (!(wrap_monitoring && monitored.shutdown)) {
        return __real_shutdown(sockfd, how);
    }
    stats.shutdown.called++;
    stats.shutdown.last_params = (struct params_shutdown_t) {
        .sockfd = sockfd,
        .how = how
    };
    if (FAIL(failures.shutdown)) {
        failures.shutdown = NEXT(failures.shutdown);
        errno = failures.shutdown_errno;
        return (stats.shutdown.last_return = failures.shutdown_ret);
    }
    failures.shutdown = NEXT(failures.shutdown);
    int ret = -1;
    ret = __real_shutdown(sockfd, how);
    return ret;
}

int __wrap_socket(int domain, int type, int protocol)
{
    if (!(wrap_monitoring && monitored.socket)) {
        return __real_socket(domain, type, protocol);
    }
    stats.socket.called++;
    stats.socket.last_params = (struct params_socket_t) {
        .domain = domain,
        .type = type,
        .protocol = protocol
    };
    if (FAIL(failures.socket)) {
        failures.socket = NEXT(failures.socket);
        errno = failures.socket_errno;
        return (stats.socket.last_return = failures.socket_ret);
    }
    failures.socket = NEXT(failures.socket);
    int ret = -2;
    ret = __real_socket(domain, type, protocol);
    return (stats.socket.last_return = ret);
}

// Additionnal functions

void reinit_network_socket_stats()
{
    memset(&(stats.accept), 0, sizeof(struct stats_accept_t));
    memset(&(stats.bind), 0, sizeof(struct stats_bind_t));
    memset(&(stats.connect), 0, sizeof(struct stats_connect_t));
    memset(&(stats.listen), 0, sizeof(struct stats_listen_t));
    memset(&(stats.poll), 0, sizeof(stats.poll));
    memset(&(stats.recv), 0, sizeof(struct stats_recv_t));
    memset(&(stats.recvfrom), 0, sizeof(struct stats_recvfrom_t));
    memset(&(stats.recvmsg), 0, sizeof(struct stats_recvmsg_t));
    memset(&(stats.recv_all), 0, sizeof(struct stats_recv_all_t));
    memset(&(stats.select), 0, sizeof(stats.select));
    memset(&(stats.send), 0, sizeof(struct stats_send_t));
    memset(&(stats.sendto), 0, sizeof(struct stats_sendto_t));
    memset(&(stats.sendmsg), 0, sizeof(struct stats_sendmsg_t));
    memset(&(stats.send_all), 0, sizeof(struct stats_send_all_t));
    memset(&(stats.shutdown), 0, sizeof(stats.shutdown));
    memset(&(stats.socket), 0, sizeof(struct stats_socket_t));
    reinit_recv_fd_table();
}

int set_recv_data(int fd, const struct recv_buffer_t *buf)
{
    if (buf == NULL) {
        return recv_remove_entry(fd);
    }
    if (buf->mode != RECV_REAL_INTERVAL &&
        buf->mode != RECV_AFTER_INTERVAL &&
        buf->mode != RECV_BEFORE_INTERVAL) {
        return -2;
    }
    bool already_there = false;
    if (fd_is_recv_buffered(fd)) {
        already_there = true;
    }
    struct recv_item *tmp = recv_get_new_entry(fd);
    if (tmp == NULL) {
        return -1;
    }
    tmp->fd = fd;
    tmp->buf = buf;
    tmp->chunk_id = 0;
    tmp->bytes_read = 0;
    getnanotime(&(tmp->last_time));
    if (buf->mode == RECV_REAL_INTERVAL && buf->nchunks > 0) {
        // The first wait interval should be that of the first chunk.
        tmp->interval = buf->chunks[0].interval;
    } else {
        tmp->interval = 0;
    }
    return (already_there ? 1 : 0);
}

