#include <netinet/in.h> // sockaddr_in
#include <sys/socket.h> // socket functions
#include <arpa/inet.h> // htons

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>

#include "rpc_client.h"

using namespace PBRPC;

void eventcb(struct bufferevent *bev, short events, void *ptr)
{
    if (events & BEV_EVENT_CONNECTED) {
		
	} else /*if (events & BEV_EVENT_ERROR)*/ {
		RpcChannel *channel = (RpcChannel *)ptr;
		channel->DisConnect();
	}
}

void *RpcClient::Connect(const std::string &con_str, google::protobuf::RpcController *controller, RpcChannel *channel) {
	size_t split_pos = con_str.find(':');
	if (split_pos == std::string::npos)	{ 
		controller->SetFailed("Error connect string : ");
		return NULL;
	}
	std::string ip_str = con_str.substr(0, split_pos);
	std::string port_str = con_str.substr(split_pos + 1);

	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	if (inet_aton(ip_str, &sin.sin_addr) == 0) {
		controller->SetFailed("Invalid connect Ip");
		return NULL;
	}
	unsigned short port;
	std::stringstream ss(port_str);
	ss >> port;
	sin.sin_port = htons(port);
	sin.sin_family = AF_INET;

	bufferevent *bev = bufferevent_socket_new(_evbase, -1, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev, readcb, NULL, eventcb, channel); 
	if (bufferevent_socket_connect(bev, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		bufferevent_free(bev);
		controller->SetFailed("Connect Failed");
		return NULL;
	}
	return bev;
}

