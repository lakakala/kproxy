#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>

#include "server.h"
#include "kmalloc.h"

void *io_thread_handler(void *arg) {
    printf("event_base_dispatch thread_io msg\n");
    struct io_thread_context *ctx = (struct io_thread_context *) arg;

    int code = event_base_loop(ctx->base, EVLOOP_NO_EXIT_ON_EMPTY);
    if (code) {
        printf("event_base_dispatch exit code %d\n", code);
    }

    return NULL;
}

void copy_data(struct evbuffer *dest, struct evbuffer *src) {
    evbuffer_add_buffer(dest, src);
}

void proxy_client_try_close(struct proxy_client *client) {

    if (client->proxy_bev == NULL) {

        if (client->in_read_status == 1 || client->in_write_status == 1) {
            printf("proxy_client_try_close only close bev\n");
            bufferevent_free(client->bev);
        }
        return;
    }

    struct evbuffer *bev_buff = bufferevent_get_output(client->bev);
    struct evbuffer *proxy_bev_buff = bufferevent_get_output(client->proxy_bev);

    int left_empty = 0;
    int right_empty = 0;
    if (client->out_write_status == 1 || (evbuffer_get_length(proxy_bev_buff) == 0 && client->in_read_status == 1)) {
        left_empty = 1;
    }

    if (client->in_write_status == 1 || (evbuffer_get_length(bev_buff) == 0 && client->out_read_status == 1)) {
        right_empty = 1;
    }

    if (left_empty == 1 && right_empty == 1) {
        printf("proxy_client_try_close close\n");
        bufferevent_free(client->bev);
        bufferevent_free(client->proxy_bev);
        return;
    }
}

void proxy_client_proxy_read_cb(struct bufferevent *bev, void *ctx) {

    struct proxy_client *client = (struct proxy_client *) ctx;

    struct evbuffer *dest = bufferevent_get_output(client->bev);
    struct evbuffer *src = bufferevent_get_input(client->proxy_bev);
    copy_data(dest, src);
    bufferevent_flush(client->bev, EV_WRITE, BEV_FLUSH);
    printf("proxy_client_proxy_read_cb\n");
}

void proxy_client_proxy_write_cb(struct bufferevent *bev, void *ctx) {
    struct proxy_client *client = (struct proxy_client *) ctx;
    printf("proxy_client_proxy_write_cb\n");

    proxy_client_try_close(client);
}

void proxy_client_proxy_event_cb(struct bufferevent *bev, short what, void *arg) {
    struct proxy_client *client = (struct proxy_client *) arg;

    if (what & BEV_EVENT_CONNECTED) {

        struct evbuffer *buff = bufferevent_get_output(client->bev);
        write_sock_connect(buff);

        bufferevent_flush(client->bev, EV_WRITE, BEV_FLUSH);
    } else if (what & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        if (what & BEV_EVENT_EOF || what & BEV_EVENT_ERROR) {

            if (what & BEV_EVENT_WRITING) {
                client->out_write_status = 1;
            }

            if (what & BEV_EVENT_READING) {
                client->out_read_status = 1;
            }

            proxy_client_try_close(client);
        }
    }
}

int proxy_client_init_req_cb(uint8_t method_len, uint8_t *methods, void *arg) {
    struct proxy_client *client = (struct proxy_client *) arg;
    struct evbuffer *buf = bufferevent_get_output(client->bev);

    int code = write_sock_init(buf, sock5_no_auth);
    if (code < 0) {
        return code;
    }

    bufferevent_flush(client->bev, EV_WRITE, BEV_FLUSH);

    return 0;
}

int proxy_client_connect_req_cb(uint8_t cmd, uint8_t address_type, uint8_t host_len,
                                char *host, uint16_t port, void *arg) {
    struct proxy_client *client = (struct proxy_client *) arg;

    client->proxy_bev = bufferevent_socket_new(client->base, -1, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(client->proxy_bev, proxy_client_proxy_read_cb, proxy_client_proxy_write_cb,
                      proxy_client_proxy_event_cb, (void *) (client));
    bufferevent_enable(client->proxy_bev, EV_WRITE | EV_READ);

    bufferevent_socket_connect_hostname(client->proxy_bev, client->dns, AF_UNSPEC, host, port);

    return 0;
}

void io_thread_read_cb(struct bufferevent *bev, void *ctx) {

    struct proxy_client *client = (struct proxy_client *) ctx;

    struct evbuffer *buf = bufferevent_get_input(client->bev);

    if (client->state == 0) {
        int result = parse_sock5(&client->parse_ctx, buf, (void *) (client),
                                 proxy_client_init_req_cb, proxy_client_connect_req_cb);
        if (result < 0) {
            // 解析失败 处理异常
            printf("parse failed code %d\n", result);
        } else if (result > 0) {
            client->state = 1;
        }
    } else {

        struct evbuffer *dest = bufferevent_get_output(client->proxy_bev);
        struct evbuffer *src = bufferevent_get_input(client->bev);
        copy_data(dest, src);
        bufferevent_flush(client->proxy_bev, EV_WRITE, BEV_FLUSH);
        printf("io_thread_read_cb\n");
    }
}

void io_thread_write_cb(struct bufferevent *bev, void *ctx) {
    struct proxy_client *client = (struct proxy_client *) ctx;

    proxy_client_try_close(client);

    printf("io_thread_write_cb\n");
}

void io_thread_event_cb(struct bufferevent *bev, short what, void *ctx) {
    struct proxy_client *client = (struct proxy_client *) ctx;

    // #define BEV_EVENT_READING	0x01	/**< error encountered while reading */
    // #define BEV_EVENT_WRITING	0x02	/**< error encountered while writing */
    // #define BEV_EVENT_EOF		0x10	/**< eof file reached */
    // #define BEV_EVENT_ERROR		0x20	/**< unrecoverable error encountered */
    // #define BEV_EVENT_TIMEOUT	0x40	/**< user-specified timeout reached */
    // #define BEV_EVENT_CONNECTED	0x80	/**< connect operation finished. */

    if (what & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        if (what & BEV_EVENT_READING) {
            client->in_read_status = 1;
        }

        if (what & BEV_EVENT_WRITING) {
            client->in_write_status = 1;
        }

        proxy_client_try_close(client);
    }
}

void proxy_sock5_read_init_req_cb(struct sock5_init_req *req, void *arg) {
    struct proxy_client *client = (struct proxy_client *) arg;


}


void io_thread_add_conn(struct io_thread_context *ctx, evutil_socket_t fd) {
    struct bufferevent *bev = bufferevent_socket_new(ctx->base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        fprintf(stderr, "Error constructing bufferevent!");
        return;
    }

    struct proxy_client *client = proxy_client_init(ctx->base, bev);

    sock5_read_init_req(bev, proxy_sock5_read_init_req_cb, client);
//    bufferevent_setcb(bev, io_thread_read_cb, io_thread_write_cb,
//                      io_thread_event_cb, (void *) (client));
//    bufferevent_enable(bev, EV_WRITE | EV_READ);
}

void proxy_server_listener_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *addr, int socklen,
                              void *ctx) {

    struct proxy_server *server = (struct proxy_server *) ctx;
    io_thread_add_conn(server->contexts[0], fd);
}

void start_io_thread(struct proxy_server *server) {

    for (int i = 0; i < IO_THREAD_NUM; i++) {
        struct io_thread_context *curr_ctx = kmalloc(sizeof(struct io_thread_context));
        curr_ctx->base = event_base_new();
        server->contexts[i] = curr_ctx;

        pthread_create(&curr_ctx->thread_id, NULL, &io_thread_handler, (void *) (curr_ctx));
    }
}

struct proxy_server *server_init() {
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(8080);

    struct proxy_server *server = kmalloc(sizeof(struct proxy_server));

    server->base = event_base_new();
    server->listener = evconnlistener_new_bind(server->base, proxy_server_listener_cb, (void *) server,
                                               LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
                                               (struct sockaddr *) &sin,
                                               sizeof(sin));

    return server;
}

void server_start(struct proxy_server *server) {
    // start io threads
    start_io_thread(server);
    event_base_dispatch(server->base);
}

struct proxy_client *proxy_client_init(struct event_base *base, struct bufferevent *bev) {
    struct proxy_client *client = kmalloc(sizeof(struct proxy_client));
    client->base = base;
    client->bev = bev;
    client->proxy_bev = NULL;
    client->dns = evdns_base_new(base, EVDNS_BASE_INITIALIZE_NAMESERVERS);
    client->state = 0;
    client->in_read_status = 0;
    client->in_write_status = 0;
    client->out_read_status = 0;
    client->out_write_status = 0;
    return client;
}

#endif
