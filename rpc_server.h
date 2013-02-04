#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include "common.h"
#include "service_mgr.h"
#include "util.h"

namespace PBRPC {
	struct Connection {
			enum STATE {ST_HEAD, ST_DATA};
			STATE _state;
			LENGTH_TYPE _data_length;
			int _id;
		};


	class RpcServer {
	private:
		RpcServiceMgr _service_mgr;
		TAllocator<Connection, SYNC::NullMutex> _conn_mgr;
		struct event_base *_ev_base;
	public:
		void NewConnection(evutil_socket_t connfd);
		void RemoveConnection(unsigned int conn_id);
		void ProcessRpcData(struct bufferevent *bev, unsigned int conn_id, struct evbuffer *input);
	public:
		RpcServer():_conn_mgr(MAX_RPC_CONNECTIONs),_ev_base(NULL) {}
		bool RegisterService(::google::protobuf::Service *service);
		void Start();
	};
}
