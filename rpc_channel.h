
#include <google/protobuf/service.h>
#include <string>
#include "rpc_client.h"
#include "common.h"

namespace PBRPC {
	class RpcChannel : public google::protobuf::RpcChannel {
	private:
		RpcClient *_client;
		string _conn_str;
		unsigned int _session_id;
		int _write_pipe, _read_pipe;
	public:
		RpcChannel(RpcClient *client, const char *connect_str);
		virtual ~RpcChannel() £û£ý
		virtual void CallMethod(const MethodDescriptor *method, RpcController *controller,
				const Message *request, const Message *response, Closure *done);

	protected:
		void Connect(google::protobuf::RpcController *controller);
	};
}
