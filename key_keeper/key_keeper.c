#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "util.h"
#include "conf.h"

/* Maximal key string length + 1. */
#define KEY_BUF_SZ (16 * 5 - 2 * sizeof(time_t))

/* Any protocol message must fit in a buffer of that size. */
#define REQ_BUF_SZ (KEY_BUF_SZ + 100)

typedef struct {
    char key[KEY_BUF_SZ];
    time_t created_ts, updated_ts;
} KeyInfo;

/* Encrypt and save given key (t) in memory location (k),
 * using (iv) as an encryption secret;
 * wipe out the given key.
 */
void update_key(void *k, KeyInfo *t, const char iv[16])
{
    t->updated_ts = time(NULL);
    encrypt_md5_cfb((char *)t, sizeof(KeyInfo)/16, iv);
    memcpy(k, t, sizeof(KeyInfo));
    memset_rnd(t, sizeof(KeyInfo));
}

/* Decrypt and load the key into (t) from memory location (k),
 * using (iv) as a decryption secret.
 */
void read_key(const void *k, KeyInfo *t, const char iv[16])
{
    memcpy(t, k, sizeof(KeyInfo));
    decrypt_md5_cfb((char *)t, sizeof(KeyInfo)/16, iv);
}

#define CONNECT_TIMEOUT 900 /* millisec */
#define SEND_TIMEOUT 300 /* millisec */
#define RECV_TIMEOUT 500 /* millisec */

/* Set up initial values for KeyInfo fields. */
void init_key(KeyInfo *key)
{
    memset_rnd(key, sizeof(KeyInfo));
    key->created_ts = key->updated_ts = 0;
    key->key[0] = 0;
}

time_t last_peer_access_ts = 0;

/* Perform a query against given peer to try to fetch the key. */
int read_peer(const char *host, int port, KeyInfo *key)
{
    char buf[REQ_BUF_SZ];
    int sock, res, ip;
    last_peer_access_ts = time(NULL);
    init_key(key);
    sock = connect_to(host, port, CONNECT_TIMEOUT, &ip);
    if (-1 == sock)
        return -1;
    if (-1 == send_str(sock, "read\n", SEND_TIMEOUT, ip, port))
        return -1;
    if (-1 == recv_str(sock, buf, sizeof(buf), RECV_TIMEOUT, ip, port))
        return -1;
    strip(buf);
    if (!strcmp("no key", buf)) {
        msg("read_peer(\"%s\", %d): no key", host, port);
        return -1;
    }
    res = sscanf(buf, "key=%70s created=%ld", key->key,
                 &key->created_ts);
    memset_rnd(buf, sizeof(buf));
    if (2 != res) {
        msg("read_peer(\"%s\", %d): wrong format", host, port);
        init_key(key);
        return -1;
    }
    msg("read_peer(\"%s\", %d): got key", host, port);
    return 0;
}

/* Send the key to given peer. */
int update_peer(const char *host, int port, const KeyInfo *key)
{
    char buf[REQ_BUF_SZ];
    int sock, res, ip;
    sock = connect_to(host, port, CONNECT_TIMEOUT, &ip);
    if (-1 == sock)
        return -1;
    sprintf(buf, "update key=%s created=%ld\n", key->key, key->created_ts);
    res = send_str(sock, buf, SEND_TIMEOUT, ip, port);
    memset_rnd(buf, sizeof(buf));
    if (-1 == res)
        return -1;
    return 0;
}

int load_from_peers(const Config *cfg, void *local_key, const char iv[16])
{
    int i, changed = 0, empty;
    KeyInfo tmp_key, peer_key;
    read_key(local_key, &tmp_key, iv);
    if (cfg)
        for (i = 0; i < PEERS_MAX; ++i)
            if (cfg->peer_hosts[i][0]) {
                read_peer(cfg->peer_hosts[i], cfg->peer_ports[i], &peer_key);
                if (peer_key.created_ts > tmp_key.created_ts) {
                    tmp_key = peer_key;
                    changed = 1;
                }
            }
    empty = !tmp_key.key[0];
    if (changed)
        update_key(local_key, &tmp_key, iv);
    else
        memset_rnd(&tmp_key, sizeof(tmp_key));
    memset_rnd(&peer_key, sizeof(peer_key));
    return !empty;
}

void format_key(char *buf, const void *local_key, const char iv[16])
{
    KeyInfo tmp_key;
    read_key(local_key, &tmp_key, iv);
    sprintf(buf, "key=%s created=%ld updated=%ld\n",
            tmp_key.key, tmp_key.created_ts, tmp_key.updated_ts);
    memset_rnd(&tmp_key, sizeof(tmp_key));
}

void process_command(int client, char *cmd, const Config *cfg,
        void *local_key, const char iv[16], int ip, int port)
{
    if (!strcmp(cmd, "ping")) {
        msg("processing command: %s", cmd);
        send_str(client, "OK\n", SEND_TIMEOUT, ip, port);
    }
    else if (!strcmp(cmd, "read") || !strcmp(cmd, "get")) {
        msg("processing command: %s", cmd);
        if (!load_from_peers(
                    !strcmp(cmd, "get") &&
                    time(NULL) - last_peer_access_ts > 0? cfg: NULL,
                    local_key, iv))
        {
            send_str(client, "no key\n", SEND_TIMEOUT, ip, port);
        }
        else {
            char buf[REQ_BUF_SZ];
            memset(buf, 0, sizeof(buf));
            format_key(buf, local_key, iv);
            send_str(client, buf, SEND_TIMEOUT, ip, port);
            memset_rnd(buf, sizeof(buf));
        }
    }
    else if (startswith(cmd, "update ")) {
        KeyInfo new_key;
        msg("processing command: update");
        init_key(&new_key);
        if (2 != sscanf(cmd + strlen("update "), "key=%70s created=%ld\n",
                    new_key.key, &new_key.created_ts))
        {
            msg("update request: bad format");
            memset_rnd(&new_key, sizeof(new_key));
        }
        else {
            update_key(local_key, &new_key, iv);
        }
    }
    else if (startswith(cmd, "set ")) {
        KeyInfo new_key;
        msg("processing command: set");
        init_key(&new_key);
        if (1 != sscanf(cmd + strlen("set "), "key=%70s\n", new_key.key))
        {
            msg("set request: bad format");
            memset_rnd(&new_key, sizeof(new_key));
        }
        else {
            int i;
            new_key.created_ts = new_key.updated_ts = time(NULL);
            for (i = 0; i < PEERS_MAX; ++i)
                if (cfg->peer_hosts[i][0])
                    update_peer(cfg->peer_hosts[i], cfg->peer_ports[i], &new_key);
            update_key(local_key, &new_key, iv);
        }
    }
    else {
        msg("unknown command!");
    }
}

int process_client(int client, const Config *cfg, void *local_key,
        const char iv[16], int ip, int port)
{
    char buf[101];
    if (-1 == recv_str(client, buf, sizeof(buf), RECV_TIMEOUT, ip, port))
        return -1;
    strip(buf);
    process_command(client, buf, cfg, local_key, iv, ip, port);
    memset_rnd(buf, sizeof(buf));
    return 0;
}

#define CONFIG_FILE "/etc/card_proxy/key_keeper.cfg"
#define LOG_FILE "/var/log/card_proxy/key_keeper.log"

int main()
{
    int sock, sz, local_key_ofs, res;
    void *p = NULL;
    KeyInfo t;
    char iv[16];
    Config cfg;
    char *config_file_name = NULL;
    char *log_file_name = NULL;

    last_peer_access_ts = time(NULL) - 100500;

    config_file_name = getenv("CONFIG_FILE");
    if (!config_file_name || !*config_file_name)
        config_file_name = CONFIG_FILE;
    if (-1 == read_config(config_file_name, &cfg)) {
        free(p);
        return 1;
    }

    log_file_name = &cfg.log_file_name[0];
    if (!*log_file_name)
        log_file_name = LOG_FILE;
        log_file = fopen(log_file_name, "a");
    if (!log_file) {
        perror("cannot open log file");
        return 1;
    }
    setvbuf(log_file, NULL, _IONBF, 0);

    res = randomize();
    if (-1 == res)
        return 1;
    memset_rnd(iv, 16);

    /* allocate local_key */
    sz = (256 * 1024) + (rand() % (1024 * 1024));
    p = malloc(sz);
    if (!p) {
        msg("main(): no memory");
        return 1;
    }
    memset_rnd(p, sz);
    local_key_ofs = (rand() % (sz - sizeof(KeyInfo))) / 3;
    init_key(&t);
    update_key((char *)p + local_key_ofs * 3, &t, iv);

    load_from_peers(&cfg, (char *)p + local_key_ofs * 3, iv);

    sock = serv_socket(cfg.bind_host, cfg.bind_port);
    if (-1 == sock) {
        free(p);
        return 1;
    }

    while (1) {
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        int client;
        char ip_str[20];

        msg("accept(): waiting");
        client = accept(sock, (struct sockaddr *)&addr, &addrlen);
        if (client == -1) {
            msg("accept(): %s", strerror(errno));
            sleep(5);
            continue;
        }
        addr2str(&addr, ip_str);
        msg("accept(): incoming connect from %s", ip_str);
        process_client(client, &cfg, (char *)p + local_key_ofs * 3, iv,
                       ntohl(addr.sin_addr.s_addr),
                       ntohs(addr.sin_port));
        shutdown(client, 2);
        close(client);
    
        randomize();
    }

    close(sock);
    free(p);
    return 0;
}

