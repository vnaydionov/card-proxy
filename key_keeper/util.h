#ifndef KK__UTIL_H__INCLUDED
#define KK__UTIL_H__INCLUDED

#include <netinet/in.h>

int startswith(const char *s, const char *t);
int is_space(char c);
void strip(char *s);

void md5_hash(const char *s, int len, char digest[16]);
void encrypt_md5_cfb(char *m, int nblocks, const char iv[16]);
void decrypt_md5_cfb(char *c, int nblocks, const char iv[16]);

void msg(const char *fmt, ...);

int randomize();
void memset_rnd(void *p, int sz);

int resolve_host(const char *host);
void fill_addr(struct sockaddr_in *addr, int ip, int port);
void ip_port2str(int ip, int port, char *str);
void addr2str(const struct sockaddr_in *addr, char *str);

int serv_socket(const char *host, int port);
int socket_non_blocking_mode(int sock, int turn_on);
int select_wait(int fd, int timeout_millisec, int read, const char *func);
void fmt_func(char *buf, const char *func, int ip, int port);
int connect_to(const char *host, int port, int timeout_millisec, int *ip_buf);
int send_str(int fd, const char *s, int timeout_millisec, int ip, int port);
int recv_str(int fd, char *buf, int buf_sz, int timeout_millisec, int ip, int port);

#endif
