#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include "common.h"
#include "service_mgr.h"

namespace PBRPC {
	class RpcServer {
		struct Connection {
			enum STATE {ST_HEAD, ST_DATA};
			STATE _state;
			LENGTH_TYPE _data_length;
			int _id;
			bufferevent *_bev;

			void *_arg;
		};

		class ConnectionMgr {
			Connection _AllConnections[MAX_RPC_CONNECTIONs];

			int _UnusedIds[MAX_RPC_CONNECTIONs];
			unsigned int _Unused_Num;
		public:
			RpcClientMgr():_Unused_Num(MAX_RPC_CONNECTIONs) {
				for (int i = 0; i < MAX_RPC_CONNECTIONs; i++) {
					_AllConnections[i]._id = i;
					_UnusedIds[i] = i;
				}
			}

			inline Connection * Add() {
				if (_Unused_Num <= 0)
					return NULL;
				return &_AllConnections[_UnusedIds[--_Unused_Num]];
			}

			inline void Remove(Connection *conn) {
				assert(_Unused_Num < MAX_RPC_CONNECTIONs);
				_UnusedIds[_Unused_Num++] = conn->_id;
			}

			inline unsigned int Total() {
				return MAX_RPC_CONNECTIONs - _Unused_Num;
			}
		};

	private:
		RpcServiceMgr _service_mgr;
		ConnectionMgr _conn_mgr;
		event_base *_ev_base;
	protected:
		void NewConnection(evutil_socket_t connfd);
		void RemoveConnection(Connection *conn);
		void ProcessRpcData(Connection *conn, struct evbuffer *input);
	public:
		bool RegisterService(::google::protobuf::Service *service);
		void Start();
	};
}