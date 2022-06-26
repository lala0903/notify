#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "notify.h"
#include "notify_server_init.h"
#include "notify_server_recv.h"
#include "notify_server_send.h"

int main(int argc, char *argv[])
{
	int ret = NotifyServerInit();

	while (ret == 0) {
		pause();
	}
	
	return 0;
}