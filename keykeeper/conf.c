#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include "util.h"
#include "conf.h"

static int read_host(char host_buf[HOST_BUF_SZ], const char *val)
{
    strncpy(host_buf, val, HOST_BUF_SZ);
    host_buf[HOST_BUF_SZ - 1] = 0;
    return 0;
}

static int read_port(int *port_buf, const char *val)
{
    if (sscanf(val, "%d", port_buf) != 1
            || *port_buf < 1024
            || *port_buf > 65534)
        return -1;
    return 0;
}

int parse_config_line(Config *cfg, const char *name, const char *val)
{
    if (!strcmp("log_file", name)) {
        strncpy(cfg->log_file_name, val, LOG_FILE_NAME_SZ - 1);
        cfg->log_file_name[LOG_FILE_NAME_SZ - 1] = 0;
        return 0;
    }
    if (!strcmp("bind_host", name))
        return read_host(cfg->bind_host, val);
    if (!strcmp("bind_port", name))
        return read_port(&cfg->bind_port, val);
    if (startswith(name, "peer")) {
        int i;
        for (i = 0; i < PEERS_MAX; ++i) {
            char param_name[20];
            sprintf(param_name, "peer%d_host", i + 1);
            if (!strcmp(param_name, name))
                return read_host(cfg->peer_hosts[i], val);
            sprintf(param_name, "peer%d_port", i + 1);
            if (!strcmp(param_name, name))
                return read_port(&cfg->peer_ports[i], val);
        }
    }
    return -1;
}

void filter_peers(Config *cfg)
{
    int i, self_ip;
    char hostname[100];
    if (-1 == gethostname(hostname, sizeof(hostname))) {
        msg("gethostname(): %s", strerror(errno));
        return;
    }
    self_ip = resolve_host(hostname);
    if (-1 == self_ip)
        return;
    for (i = 0; i < PEERS_MAX; ++i)
        if (cfg->peer_hosts[i][0]) {
            int peer_ip;
            peer_ip = resolve_host(cfg->peer_hosts[i]);
            if (-1 == peer_ip) {
                cfg->peer_hosts[i][0] = 0;
                continue;
            }
            if ((self_ip == peer_ip || (peer_ip & 0xff000000) == 0x7f000000)
                    && cfg->bind_port == cfg->peer_ports[i])
                cfg->peer_hosts[i][0] = 0;
        }
}

int read_config(const char *cfg_fname, Config *cfg)
{
    FILE *f;
    int i;

    memset(cfg, 0, sizeof(Config));

    f = fopen(cfg_fname, "r");
    if (!f) {
        msg("read_config(\"%s\"): %s", cfg_fname, strerror(errno));
        return -1;
    }

    for (i = 0; ; ) {
        char line[CFG_LINE_BUF_SZ];
        char *eq, *val;

        if (!fgets(line, sizeof(line), f))
            break;
        line[sizeof(line) - 1] = 0;
        ++i;
        strip(line);
        if ('#' == line[0] || !line[0])
            continue;
        eq = strchr(line, '=');
        val = eq + 1;
        if (!eq || eq == line || !*val) {
            msg("read_config(\"%s\"): line %d: %s", cfg_fname, i,
                "wrong format");
            fclose(f);
            return -1;
        }
        *eq = 0;

        if (parse_config_line(cfg, line, val) != 0) {
            msg("read_config(\"%s\"): line %d: %s", cfg_fname, i,
                "wrong format");
            fclose(f);
            return -1;
        }
    }
    fclose(f);

    if (!cfg->bind_host[0] || !cfg->bind_port)
    {
        msg("read_config(\"%s\"): line %d: %s", cfg_fname, i,
            "missing params");
        return -1;
    }
    filter_peers(cfg);
    for (i = 0; i < PEERS_MAX; ++i)
        if (cfg->peer_hosts[i][0])
            msg("peer: %s:%d", cfg->peer_hosts[i], cfg->peer_ports[i]);

    return 0;
}
