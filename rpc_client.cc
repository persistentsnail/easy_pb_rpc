#include <netinet/in.h> // sockaddr_in
#include <sys/socket.h> // socket functions

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

using namespace PBRPC;

int RpcClient::Connect(const char *con_str) {

}
