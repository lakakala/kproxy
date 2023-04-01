#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>

#include "server.h"
#include "kmalloc.h"

void *io_thread_handler(void *arg)
{
	printf("event_base_dispatch thread_io msg\n");
	struct io_thread_context *ctx = (struct io_thread_context *)arg;

	int code = event_base_loop(ctx->base, EVLOOP_NO_EXIT_ON_EMPTY);
	if (code)
	{
		printf("event_base_dispatch exit code %d\n", code);
	}
}

void io_thread_write_cb(struct bufferevent *bev, void *ctx)
{
	printf("hello world\n");
}

void io_thread_read_cb(struct bufferevent *bev, void *ctx)
{

	struct proxy_client *client = (struct proxy_client *)ctx;
	
	char data[1024];
	size_t len = bufferevent_read(bev, data, 1023);
	data[len] = '\0';
	printf("%s\n", data);
}

void io_thread_add_conn(struct io_thread_context *ctx, evutil_socket_t fd)
{
	struct bufferevent *bev = bufferevent_socket_new(ctx->base, fd, BEV_OPT_CLOSE_ON_FREE);
	if (!bev)
	{
		fprintf(stderr, "Error constructing bufferevent!");
		return;
	}

	bufferevent_setcb(bev, io_thread_read_cb, io_thread_write_cb, NULL, (void *)(ctx));
	bufferevent_enable(bev, EV_WRITE | EV_READ);
}

void proxy_server_listener_cb(struct evconnlistener *, evutil_socket_t fd, struct sockaddr *, int socklen, void *ctx)
{

	struct proxy_server *server = (struct proxy_server *)ctx;
	printf("hello world\n");
	io_thread_add_conn(server->contexts[0], fd);
}

void start_io_thread(struct proxy_server *server)
{

	for (int i = 0; i < IO_THREAD_NUM; i++)
	{
		struct io_thread_context *curr_ctx = kmalloc(sizeof(struct io_thread_context));
		curr_ctx->base = event_base_new();
		server->contexts[i] = curr_ctx;

		pthread_create(&curr_ctx->thread_id, NULL, &io_thread_handler, (void *)(curr_ctx));
	}
}

struct proxy_server *server_init()
{
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(8080);

	struct proxy_server *server = kmalloc(sizeof(struct proxy_server));

	server->base = event_base_new();
	server->listener = evconnlistener_new_bind(server->base, proxy_server_listener_cb, (void *)server,
											   LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
											   (struct sockaddr *)&sin,
											   sizeof(sin));

	return server;
}

void server_start(struct proxy_server *server)
{
	// start io threads
	start_io_thread(server);
	event_base_dispatch(server->base);
}

#endif
