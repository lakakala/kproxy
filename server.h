#include <pthread.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <stdlib.h>

#define IO_THREAD_NUM 10

struct io_thread_context
{
	pthread_t thread_id;
	struct event_base *base;
};

struct proxy_client;

struct proxy_server
{
	struct event_base *base;
	struct evconnlistener *listener;
	struct io_thread_context *contexts[IO_THREAD_NUM];
};

struct proxy_client
{
	struct bufferevent *bev;
	struct proxy_server *server;
};

struct proxy_server *server_init();

void server_start(struct proxy_server *);
