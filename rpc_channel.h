
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <string>
#include "rpc_client.h"
#include "common.h"

namespace PBRPC {
	using google::protobuf::Message;
	using google::protobuf::Closure;
	class RpcChannel : public google::protobuf::RpcChannel {
	private:
		RpcClient *_client;
		std::string _conn_str;
		unsigned int _session_id;
		int _write_pipe, _read_pipe;
	public:
		RpcChannel(RpcClient *client, const char *connect_str);
		virtual ~RpcChannel(); 
		virtual void CallMethod(const google::protobuf::MethodDescriptor *method, google::protobuf::RpcController *controller, const Message *request,Message *response, Closure *done);

	protected:
		void Connect(google::protobuf::RpcController *controller);
	};
}
