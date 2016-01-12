#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <openssl/md5.h>

#include "util.h"

#define DEV_RANDOM "/dev/urandom"

FILE *log_file = NULL;

int startswith(const char *s, const char *t)
{
    int len_s, len_t;
    len_s = strlen(s);
    len_t = strlen(t);
    return len_t <= len_s && !strncmp(s, t, len_t);
}

int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

void strip(char *s)
{
    int i, j, k;
    for (i = 0; is_space(s[i]); ++i);
    for (j = strlen(s) - 1; j >= 0 && is_space(s[j]); --j);
    if (i > 0) {
        for (k = i; k <= j; ++k)
            s[k - i] = s[k];
        s[k - i] = 0;
    }
    else
        s[j + 1] = 0;
}

void md5_hash(const char *s, int len, char digest[16])
{
    MD5_CTX md5;
    MD5_Init(&md5);
    MD5_Update(&md5, s, len);
    MD5_Final((unsigned char *)digest, &md5);
}

void encrypt_md5_cfb(char *m, int nblocks, const char iv[16])
{
    int i, j;
    char a[16], digest[16];
    memcpy(a, iv, 16);
    for (j = 0; j < nblocks; ++j) {
        md5_hash(a, 16, digest);
        for (i = 0; i < 16; ++i) {
            a[i] = (m[j * 16 + i] ^= digest[i]);
        }
    }
}

void decrypt_md5_cfb(char *c, int nblocks, const char iv[16])
{
    int i, j;
    char a[16], digest[16];
    memcpy(a, iv, 16);
    for (j = 0; j < nblocks; ++j) {
        md5_hash(a, 16, digest);
        for (i = 0; i < 16; ++i) {
            a[i] = c[j * 16 + i];
            c[j * 16 + i] ^= digest[i];
        }
    }
}

void msg(const char *fmt, ...)
{
    time_t t;
    struct tm tx;
    va_list argptr;

    if (!log_file)
        return;
    va_start(argptr, fmt);
    t = time(NULL);
    memcpy(&tx, localtime(&t), sizeof(tx));
    fprintf(log_file, "%04d-%02d-%02d %02d:%02d:%02d ",
            tx.tm_year + 1900, tx.tm_mon + 1, tx.tm_mday,
            tx.tm_hour, tx.tm_min, tx.tm_sec);
    vfprintf(log_file, fmt, argptr);
    fprintf(log_file, "\n");
    va_end(argptr);
}

int randomize()
{
    int fd, res;
    fd = open(DEV_RANDOM, O_RDONLY);
    if (-1 == fd) {
        msg("open(\"%s\"): %s", DEV_RANDOM, strerror(errno));
        return -1;
    }
    if (read(fd, &res, sizeof(res)) != sizeof(res)) {
        msg("read(\"%s\"): %s", DEV_RANDOM, "short read");
        close(fd);
        return -1;
    }
    close(fd);
    srand(abs(res) % RAND_MAX);
    rand();
    rand();
    return 0;
}

void memset_rnd(void *p, int sz)
{
    int i;
    char *s = (char *)p;
    for (i = 0; i < sz; ++i)
        s[i] = rand();
}

int resolve_host(const char *host)
{
    struct hostent *h;
    h = gethostbyname(host);
    if (!h) {
        msg("resolve_host(\"%s\"): %s", host, hstrerror(h_errno));
        return -1;
    }
    return ntohl(*(int *)(h->h_addr_list[0]));
}

void fill_addr(struct sockaddr_in *addr, int ip, int port)
{
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port & 0xffff);
    addr->sin_addr.s_addr = htonl(ip);
}

void ip_port2str(int ip, int port, char *str)
{
    sprintf(str, "%d.%d.%d.%d:%d",
            (ip >> 24) & 0xff,
            (ip >> 16) & 0xff,
            (ip >> 8) & 0xff,
            ip & 0xff, port);
}

void addr2str(const struct sockaddr_in *addr, char *str)
{
    ip_port2str(ntohl(addr->sin_addr.s_addr), ntohs(addr->sin_port), str);
}

static int close_if_open(int fd)
{
    if (-1 != fd)
        close(fd);
    return -1;
}

static int process_errno(const char *func, const char *call, int fd)
{
    if (func && call)
        msg("%s: %s: %s", func, call, strerror(errno));
    return close_if_open(fd);
}

int serv_socket(const char *host, int port)
{
    char func[100];
    int ip, sock, one = 1;
    struct sockaddr_in addr;

    ip = resolve_host(host);
    if (-1 == ip)
        return -1;
    fmt_func(func, "serv_socket", ip, port);
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sock)
        return process_errno(func, "socket()", sock);
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                   (char *)&one, sizeof(one)) == -1)
        return process_errno(func, "setsockopt(SO_REUSEADDR)", sock);
    fill_addr(&addr, ip, port);
    if (-1 == bind(sock, (struct sockaddr *)&addr, sizeof(addr)))
        return process_errno(func, "bind()", sock);
    if (-1 == listen(sock, 5))
        return process_errno(func, "listen()", sock);
    msg("listening at %s:%d", host, port);
    return sock;
}

int socket_non_blocking_mode(int sock, int turn_on)
{
    int mode, old;
    mode = fcntl(sock, F_GETFL, NULL);
    if (-1 == mode)
        return process_errno("socket_non_blocking_mode()", "fcntl(F_GETFL)", -1);
    old = (mode & O_NONBLOCK) != 0;
    mode &= ~O_NONBLOCK;
    if (turn_on)
        mode |= O_NONBLOCK;
    if (-1 == fcntl(sock, F_SETFL, mode))
        return process_errno("socket_non_blocking_mode()", "fcntl(F_SETFL)", -1);
    return old;
}

int select_wait(int fd, int timeout_millisec, int read, const char *func)
{
    int res;
    fd_set fds;
    fd_set *rfds = NULL, *wfds = NULL;
    struct timeval timeout;

    timeout.tv_sec = timeout_millisec / 1000;
    timeout.tv_usec = (timeout_millisec % 1000) * 1000;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    if (read)
        rfds = &fds;
    else
        wfds = &fds;
    res = select(fd + 1, rfds, wfds, NULL, &timeout);
    if (-1 == res)
        return process_errno(func, "select()", -1);
    if (0 == res || !FD_ISSET(fd, &fds)) {
        msg("%s: timed out", func);
        return -1;
    }
    return 0;
}

void fmt_func(char *buf, const char *func, int ip, int port)
{
    strcpy(buf, func);
    strcat(buf, "(");
    if (ip)
        ip_port2str(ip, port, buf + strlen(buf));
    strcat(buf, ")");
}

int connect_to(const char *host, int port, int timeout_millisec, int *ip_buf)
{
    char func[100];
    int ip, sock, res, optval = 0;
    socklen_t optlen = sizeof(optval);
    struct sockaddr_in addr;

    ip = resolve_host(host);
    if (-1 == ip)
        return -1;
    *ip_buf = ip;
    fmt_func(func, "connect_to", ip, port);
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sock)
        return process_errno(func, "socket()", sock);
    if (-1 == socket_non_blocking_mode(sock, 1))
        return close_if_open(sock);
    fill_addr(&addr, ip, port);
    res = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (!(-1 == res && EINPROGRESS == errno))
        return process_errno(func, "connect()", sock);
    if (-1 == select_wait(sock, timeout_millisec, 0, func))
        return close_if_open(sock);
    if (-1 == getsockopt(sock, SOL_SOCKET, SO_ERROR, &optval, &optlen))
        return process_errno(func, "getsockopt(SO_ERROR)", sock);
    if (optval) {
        msg("%s: connect(): %s", func, strerror(optval));
        return close_if_open(sock);
    }
    if (-1 == socket_non_blocking_mode(sock, 0))
        return close_if_open(sock);
    return sock;
}

int send_str(int fd, const char *s, int timeout_millisec, int ip, int port)
{
    int i = 0, len;
    char func[100];

    fmt_func(func, "send_str", ip, port);
    len = strlen(s);
    while (i < len) {
        int n;
        if (-1 == select_wait(fd, timeout_millisec, 0, func))
            return -1;
        n = write(fd, &s[i], len - i);
        if (-1 == n)
            return process_errno(func, "write()", -1);
        if (n < 1) {
            msg("%s: short write", func);
            return -1;
        }
        i += n;
    }
    return 0;
}

int recv_str(int fd, char *buf, int buf_sz, int timeout_millisec, int ip, int port)
{
    int sz = 0;
    char func[100];

    fmt_func(func, "recv_str", ip, port);
    while (1) {
        int n, left = buf_sz - sz - 1;
        if (left < 1) {
            msg("%s: buf_sz is too small", func);
            return -1;
        }
        if (-1 == select_wait(fd, timeout_millisec, 1, func))
            return -1;
        n = read(fd, &buf[sz], left);
        if (-1 == n)
            return process_errno(func, "read()", -1);
        sz += n;
        buf[sz] = 0;
        if (strchr(buf, '\n'))
            return 0;
        if (n < 1) {
            msg("%s: nl not found", func);
            return -1;
        }
    }
}
