#include <netinet/in.h> // sockaddr_in
#include <sys/socket.h> // socket functions

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

struct RpcClient
{
	enum STATE {ST_HEAD, ST_DATA};
	STATE _state;
	LENGTH_TYPE _data_length;
	int _id;
	bufferevent *_bev;
};

class RpcClientMgr
{
	RpcClient _AllClients[MAX_RPC_CLIENTs];

	int _UnusedIds[MAX_RPC_CLIENTs];
	unsigned int _Unused_Num;
public:
	RpcClientMgr():_Unused_Num(MAX_RPC_CLIENTs) 
	{
		for (int i = 0; i < MAX_RPC_CLIENTs; i++)
		{
			_AllClients[i]._id = i;
			_UnusedIds[i] = i;
		}
	}

	inline RpcClient * Add()
	{
		if (_Unused_Num <= 0)
			return NULL;
		return &_AllClients[_UnusedIds[--_Unused_Num]];
	}

	inline void Remove(RpcClient *clt)
	{
		assert(_Unused_Num < MAX_RPC_CLIENTs);
		_UnusedIds[_Unused_Num++] = clt->_id;
	}

	inline unsigned int Total()
	{
		return MAX_RPC_CLIENTs - _Unused_Num;
	}
};

RpcClientMgr g_CltsMgr;


inline void ReadCallback(struct bufferevent *bev, void *arg)
{
	struct evbuffer *input, *output;
	RpcClient *clt = (RpcClient *)arg;
	size_t buf_len;

	input = bufferevent_get_input(bev);
	buf_len = evbuffer_get_length(input);

	for (;;) {
		switch (clt->_state) {
			case RpcClient::ST_HEAD : {
				if (buf_len < HEAD_LEN) return;
				evbuffer_remove(input, &clt->_data_length, HEAD_LEN);
				clt->_state = RpcClient::ST_HEAD;
				break;
			}
			case RpcClient::ST_DATA : {
				if (buf_len < clt->_data_length) return;
				HandleRpcCall(evbuffer_pullup(input, clt->_data_length), clt->_data_length);	
				evbuffer_drain(input, clt->_data_length);
				clt->_state = RpcClient::ST_HEAD;
				break;
			}
			default: {
				ERR_LOG("Error Client State!");	
				break;
			}
		}
	}
}

inline void ErrorCallback(struct bufferevent *bev, short error, void *arg)
{
	if (error & BEV_EVENT_EOF) {
		RpcClient *clt = (RpcClient *)arg;
		g_CltsMgr.Del(clt);
	} else {
		ERR_LOG("Error Callback!");
	}
	bufferevent_free(bev);
}

inline void DoAccept(evutil_socket_t listener, short event, void *arg)
{
	struct event_base *ev_base = (struct event_base *)arg;
	struct sockaddr_storage ss;
	socklen_t slen = sizeof(ss);
	evutil_socket_t client = accept(listener, (struct sockaddr *)&ss, &slen);
	if (client < 0)
		ErrorRet("accept");
	else {
		struct 	bufferevent *bev;
		RpcClient *clt;
		if ((clt = g_CltsMgr.Add()) == NULL) 
			return;
		clt->_state = RpcClient::ST_HEAD;
		evutil_make_socket_nonblocking(client);
		bev = bufferevent_socket_new(ev_base, client, BEV_OPT_CLOSE_ON_FREE);
		clt->_bev = bev;
		bufferevent_setcb(bev, ReadCallback, NULL, ErrorCallback, clt);
	}
}

inline int RpcDataSender(char *data_buf, size_t length, void *caller) 
{
	RpcClient *clt = (RpcClient *)caller;
	evbuffer *output = bufferevent_get_output(clt->_bev);
	LENGTH_TYPE header = length;
	evbuffer_add(output, &header, HEAD_LEN); 
	evbuffer_add(output, data_buf, length);
	return 1;
}

int main(int argc, char *argv[])
{
	evutil_socket_t listener;
	struct sockaddr_in sin;
	struct event_base *ev_base;
	struct event *listener_event;

	ev_base = event_base_new();
	if (!ev_base)
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
	listener_event = event_new(ev_base, listener, EV_READ | EV_PERSIST, DoAccept, (void*)ev_base);
	event_add(listener_event, NULL);
	event_base_dispatch(ev_base);
	return 0;
}
