
#include <google/protobuf/service.h>
#include <string>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

namespace PBRPC {
	class RpcChannel;
	class RpcClient {
			struct event_base *_evbase;	
		public:
			RpcClient() { _evbase = event_base_new(); }
			~RpcClient() { event_base_free(_evbase); }

			void * Connect(const std::string &con_str, google::protobuf::RpcController *controller);
			void DisConnect(RpcChannel *channel);
			void DoRpc(RpcChannel *channel, char *rpc_data, size_t length);
			void Wait();
	};
}
