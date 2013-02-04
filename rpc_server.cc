#include <netinet/in.h> // sockaddr_in
#include <sys/socket.h> // socket functions

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rpc_server.h"
#include "svc_name2id.h"
#include "rpc_controller.h"

using namespace PBRPC;

struct ARGDATA {
	RpcServer *_server;
	unsigned int _conn_id;
};

inline void ReadCallback(struct bufferevent *bev, void *arg) {
	struct evbuffer *input, *output;
	RpcServer *svr = ((ARGDATA *)arg)->_server;
	unsigned int conn_id = ((ARGDATA *)arg)->_conn_id;

	input = bufferevent_get_input(bev);
	svr->ProcessRpcData(bev, conn_id, input);
}

inline void ErrorCallback(struct bufferevent *bev, short error, void *arg) {
	if (error & BEV_EVENT_EOF) {
		RpcServer *svr = ((ARGDATA *)arg)->_server;
		unsigned int conn_id = ((ARGDATA *)arg)->_conn_id;
		svr->RemoveConnection(conn_id);
	} else {
		ERR_LOG("Error Callback!");
	}
	bufferevent_free(bev);
}

inline void DoAccept(evutil_socket_t listener, short event, void *arg) {
	RpcServer *server = (RpcServer *)arg;
	struct sockaddr_storage ss;
	socklen_t slen = sizeof(ss);
	evutil_socket_t client = accept(listener, (struct sockaddr *)&ss, &slen);
	if (client < 0) 
		perror("accept");
	else {
		server->NewConnection(client);
	}
}

bool RpcServer::RegisterService(::google::protobuf::Service *service) {
	const google::protobuf::ServiceDescriptor *sd = service->GetDescriptor();
	unsigned int svc_id = SERVICE_NAME2ID::instance()->RpcServiceName2Id(sd->name().c_str());
	return _service_mgr.RegisterRpcService(service, svc_id);
}

void RpcServer::Start() {
	evutil_socket_t listener;
	struct sockaddr_in sin;
	struct event *listener_event;

	_ev_base = event_base_new();
	if (!_ev_base)
		ErrorDie("event_base_new");

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(RPC_SVR_PORT);

	listener = socket(AF_INET, SOCK_STREAM, 0);
	evutil_make_socket_nonblocking(listener);
	evutil_make_listen_socket_reuseable(listener);

	if (bind(listener, (struct sockaddr*)&sin, sizeof(sin)) < 0)
		ErrorDie("bind");

	if (listen(listener, 128) < 0)
		ErrorDie("listener");
	listener_event = event_new(_ev_base, listener, EV_READ | EV_PERSIST, DoAccept, (void*)this);
	event_add(listener_event, NULL);
	event_base_dispatch(_ev_base);

	event_base_free(_ev_base);
}

void RpcServer::NewConnection(evutil_socket_t connfd) {
	struct 	bufferevent *bev;
	unsigned int conn_id;
	if ((conn_id = _conn_mgr.Alloc()) == 0) {
		ERR_LOG("No connection Id is useable");
		return;
	}
	Connection *conn = _conn_mgr.Get(conn_id);
	conn->_state = Connection::ST_HEAD;
	evutil_make_socket_nonblocking(connfd);
	bev = bufferevent_socket_new(_ev_base, connfd, BEV_OPT_CLOSE_ON_FREE);

	ARGDATA *arg = new ARGDATA;
	arg->_server = this;
	arg->_conn_id = conn_id;
	bufferevent_setcb(bev, ReadCallback, NULL, ErrorCallback, arg);
	bufferevent_enable(bev, EV_READ | EV_WRITE);
}

void RpcServer::RemoveConnection(unsigned int conn_id) {
	_conn_mgr.Free(conn_id);
}

void RpcServer::ProcessRpcData(struct bufferevent *bev, unsigned int conn_id, struct evbuffer *input) {
	size_t buf_len;
	Connection *conn = _conn_mgr.Get(conn_id);
	if (!conn) {
		ERR_LOG("ProcessRpcData Failed : No Id is %d of the Connections", conn_id);
		return;
	}

	for (;;) {
		switch (conn->_state) {
			case Connection::ST_HEAD : {
				buf_len = evbuffer_get_length(input);
				if (buf_len < HEAD_LEN) return;
				evbuffer_remove(input, &conn->_data_length, HEAD_LEN);
				conn->_state = Connection::ST_DATA;
				break;
			}
			case Connection::ST_DATA : {
				buf_len = evbuffer_get_length(input);
				if (buf_len < conn->_data_length) return;
				std::string ret_str;
				RpcController rpc_controller;
				_service_mgr.HandleRpcCall(evbuffer_pullup(input, conn->_data_length), conn->_data_length, ret_str, &rpc_controller);
				if (rpc_controller.Failed()) {
					ERR_LOG("Process Rpc Call Failed : %s", rpc_controller.ErrorText().c_str());
				} else {
					evbuffer *output = bufferevent_get_output(bev);
					LENGTH_TYPE header = ret_str.size();
					evbuffer_add(output, &header, HEAD_LEN); 
					evbuffer_add(output, ret_str.c_str(), ret_str.size());
				}
				evbuffer_drain(input, conn->_data_length);
				conn->_state = Connection::ST_HEAD;
				break;
			}
			default: {
				ERR_LOG("Error Connection State!");	
				break;
			}
		}
	}
}
