#include <pthread.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/dns.h>
#include <stdlib.h>

#include "sock5.h"

#define IO_THREAD_NUM 10

struct io_thread_context {
    pthread_t thread_id;
    struct event_base *base;
};

struct proxy_server {
    struct event_base *base;
    struct evconnlistener *listener;
    struct io_thread_context *contexts[IO_THREAD_NUM];
};

struct proxy_server *server_init();

void server_start(struct proxy_server *);

struct proxy_client {

    uint8_t state;

    uint8_t in_read_status;
    uint8_t out_read_status;
    uint8_t in_write_status;
    uint8_t out_write_status;
    struct event_base *base;
    struct evdns_base *dns;
    struct bufferevent *bev;
    struct bufferevent *proxy_bev;

    struct sock5_parse_context parse_ctx;
};

struct proxy_client *proxy_client_init(struct event_base *base, struct bufferevent *bev);