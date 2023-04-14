#ifndef SOCK5_PARSER_C
#define SOCK5_PARSER_C

#include "sock5.h"
#include "kmalloc.h"
#include <string.h>


int write_sock_init(struct evbuffer *buf, uint8_t method) {
    uint8_t ver = SOCK5_VER;
    evbuffer_add(buf, &ver, sizeof(uint8_t));
    evbuffer_add(buf, &method, sizeof(method));
    return 0;
}

int write_sock_connect(struct evbuffer *buf) {
    //         +----+-----+-------+------+----------+----------+
    //        |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
    //        +----+-----+-------+------+----------+----------+
    //        | 1  |  1  | X'00' |  1   | Variable |    2     |
    //        +----+-----+-------+------+----------+----------+
    uint8_t ver = SOCK5_VER;
    evbuffer_add(buf, &ver, sizeof(uint8_t));
    uint8_t reply = 0x00;
    evbuffer_add(buf, &reply, sizeof(reply));
    uint8_t rsv = 0x00;
    evbuffer_add(buf, &rsv, sizeof(rsv));
    uint8_t address_type = sock5_address_ipv4;
    evbuffer_add(buf, &address_type, sizeof(address_type));

    uint8_t ip[4];
    evbuffer_add(buf, ip, 4);

    uint16_t port;
    evbuffer_add(buf, &port, 2);
    return 0;
}

int parse_sock5(struct sock5_parse_context *ctx, struct evbuffer *buf, void *arg,
                sock5_init_req_cb init_cb, sock5_connect_req_cb connect_cb) {
    enum state {
        // init_req
        sw_init_start = 0,
        sw_method_body,

        // connect_req
        sw_connect_start,
        sw_connect_ipv4,
        sw_connect_host_len,
        sw_connect_host,
        sw_connect_ipv6,
        sw_connect_port,
    };

    for (;;) {
        size_t len = evbuffer_get_length(buf);

        switch (ctx->state) {
            case sw_init_start: {

                if (len < 2) {
                    return 0;
                }

                uint8_t version;
                evbuffer_remove(buf, &version, sizeof(uint8_t));

                if (version != SOCK5_VER) {
                    return -1;
                }

                evbuffer_remove(buf, &ctx->method_len, sizeof(uint8_t));

                ctx->methods = kmalloc(ctx->method_len);
                ctx->state = sw_method_body;
                break;
            }
            case sw_method_body: {
                if (len < ctx->method_len) {
                    return 0;
                }

                evbuffer_remove(buf, ctx->methods, ctx->method_len);

                ctx->state = sw_connect_start;
                return init_cb(ctx->method_len, ctx->methods, arg);
            }

            case sw_connect_start: {

                if (len < 4) {
                    return 0;
                }

                uint8_t version;
                evbuffer_remove(buf, &version, sizeof(uint8_t));

                if (version != SOCK5_VER) {
                    return -1;
                }

                evbuffer_remove(buf, &ctx->cmd, sizeof(uint8_t));

                evbuffer_remove(buf, &ctx->rsv, sizeof(uint8_t));

                evbuffer_remove(buf, &ctx->address_type, sizeof(uint8_t));

                if (ctx->address_type == sock5_address_ipv4) {
                    ctx->state = sw_connect_ipv4;
                    break;
                } else if (ctx->address_type == sock5_address_host) {
                    ctx->state = sw_connect_host_len;
                    break;
                } else if (ctx->address_type == sock5_address_ipv6) {
                    ctx->state = sw_connect_ipv6;
                    break;
                } else {
                    return -1;
                }
            }
            case sw_connect_ipv4: {
                return -1;
            }
            case sw_connect_ipv6: {
                return -1;
            }
            case sw_connect_host_len: {
                if (len < 1) {
                    return 0;
                }

                evbuffer_remove(buf, &ctx->host_len, sizeof(uint8_t));

                ctx->host = kmalloc(ctx->host_len + 1);
                ctx->state = sw_connect_host;
                break;
            }
            case sw_connect_host: {
                if (len < ctx->host_len) {
                    return 0;
                }
                evbuffer_remove(buf, ctx->host, ctx->host_len);
                ctx->host[ctx->host_len] = '\0';
                ctx->state = sw_connect_port;
                break;
            }
            case sw_connect_port: {
                if (len < 2) {
                    return 0;
                }

                evbuffer_remove(buf, &ctx->port, sizeof ctx->port);
                ctx->port = ntohs(ctx->port);
                int code = connect_cb(ctx->cmd, ctx->address_type, ctx->host_len, ctx->host, ctx->port, arg);
                if (code < 0) {
                    return code;
                } else {
                    return 1;
                }
            }
        }
    }
}

#endif // SOCK5_PARSER_C
