#ifndef KK__CONF_H__INCLUDED
#define KK__CONF_H__INCLUDED

#define HOST_BUF_SZ 100
#define CFG_LINE_BUF_SZ (HOST_BUF_SZ + 40)
#define PEERS_MAX 5

typedef struct {
    char bind_host[HOST_BUF_SZ];
    int bind_port;
    char peer_hosts[PEERS_MAX][HOST_BUF_SZ];
    int peer_ports[PEERS_MAX];
} Config;

int parse_config_line(Config *cfg, const char *name, const char *val);
void filter_peers(Config *cfg);
int read_config(const char *cfg_fname, Config *cfg);

#endif
