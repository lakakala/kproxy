#include <stdio.h>
#include "server.h"

int main()
{
	printf("hello world\n");
	struct proxy_server* server = server_init();

	server_start(server);
	return 0;
}
