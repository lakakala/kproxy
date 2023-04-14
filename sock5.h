#include <stddef.h>
#include <stdint.h>
#include <event2/buffer.h>

#define SOCK5_VER  0x05

enum auth_method_type {
    sock5_no_auth = 0x00,
    sock5_user_passwd = 0x02,
};

enum sock5_command_type {
    sock5_command_connect = 0x01,
    sock5_command_bind = 0x02,
    sock5_command_associate = 0x03,
};

enum sock5_address_type {
    sock5_address_ipv4 = 0x01,
    sock5_address_host = 0x03,
    sock5_address_ipv6 = 0x04,
};

struct sock5_parse_context {
    uint8_t state;
    uint8_t method_len;
    uint8_t *methods;

    uint8_t cmd;
    uint8_t rsv;
    uint8_t address_type;
    uint8_t host_len;
    char *host;
    uint16_t port;
};

typedef int (*sock5_init_req_cb)(uint8_t method_len, uint8_t *methods, void *arg);

typedef int (*sock5_connect_req_cb)(uint8_t cmd, uint8_t address_type, uint8_t host_len, char *host, uint16_t port,
                                    void *arg);


int parse_sock5(struct sock5_parse_context *ctx, struct evbuffer *buf, void *arg,
                sock5_init_req_cb init_cb, sock5_connect_req_cb connect_cb);

int write_sock_init(struct evbuffer *buf, uint8_t method);

int write_sock_connect(struct evbuffer *buf);