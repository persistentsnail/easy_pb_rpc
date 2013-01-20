#include <netinet/in.h> // sockaddr_in
#include <sys/socket.h> // socket functions

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace PBRPC;

struct ARGDATA {
	RpcServer *_server;
	Connection *_conn;
};

inline void ReadCallback(struct bufferevent *bev, void *arg)
{
	struct evbuffer *input, *output;
	RpcServer *svr = ((ARGDATA *)arg)->_server;
	Connection *conn = ((ARGDATA *)arg)->_conn;

	input = bufferevent_get_input(bev);
	svr->ProcessRpcData(conn, input);
}

inline void ErrorCallback(struct bufferevent *bev, short error, void *arg)
{
	if (error & BEV_EVENT_EOF) {
		RpcServer *svr = ((ARGDATA *)arg)->_server;
		Connection *conn = ((ARGDATA *)arg)->_conn;
		svr->RemoveConnection(conn);
	} else {
		ERR_LOG("Error Callback!");
	}
	bufferevent_free(bev);
}

inline void DoAccept(evutil_socket_t listener, short event, void *arg)
{
	RpcServer *server = (RpcServer *)arg;
	struct sockaddr_storage ss;
	socklen_t slen = sizeof(ss);
	evutil_socket_t client = accept(listener, (struct sockaddr *)&ss, &slen);
	if (client < 0)
		ErrorRet("accept");
	else {
		server->NewConnection(client);
	}
}

bool RpcServer::RegisterService(::google::protobuf::Service *service) {
	return _service_mgr.RegisterRpcService(service);
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
	sin.sin_port = htons(RPC_SRV_PORT);

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
	Connection *conn;
	if ((conn = _conn_mgr.Add()) == NULL) 
		return;
	conn->_state = Connection::ST_HEAD;
	evutil_make_socket_nonblocking(connfd);
	bev = bufferevent_socket_new(_ev_base, connfd, BEV_OPT_CLOSE_ON_FREE);
	conn->_bev = bev;

	ARGDATA *arg = new ARGDATA;
	arg->_server = this;
	arg->_conn = conn;
	conn->_arg = arg;
	bufferevent_setcb(bev, ReadCallback, NULL, ErrorCallback, arg);
}

void RpcServer::RemoveConnection(Connection *conn) {
	delete (ARGDATA *)conn->_arg;
	_conn_mgr.Remove(conn);
}

void RpcServer::ProcessRpcData(Connection *conn, struct evbuffer *input) {
	size_t buf_len = evbuffer_get_length(input);

	for (;;) {
		switch (conn->_state) {
			case Connection::ST_HEAD : {
				if (buf_len < HEAD_LEN) return;
				evbuffer_remove(input, &conn->_data_length, HEAD_LEN);
				conn->_state = Connection::ST_HEAD;
				break;
			}
			case Connection::ST_DATA : {
				if (buf_len < conn->_data_length) return;
				std::string ret_str;
				_service_mgr->RpcCallHandle(evbuffer_pullup(input, conn->_data_length), conn->_data_length, ret_str);
				google::protobuf::RpcControl *_rpc_control = _service_mgr->GetRpcControl();
				if (_rpc_control->Failed()) {
					ERR_LOG("Process Rpc Call Failed : %s", _rpc_control->ErrorText());
				} else {
					evbuffer *output = bufferevent_get_output(conn->_bev);
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