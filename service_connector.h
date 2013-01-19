
#include <map>
#include <google/protobuf/service.h>
#include "common.h"


class RpcServiceConnector : public google::protobuf::RpcChannel {
private:
	RpcSendFuncType _sender;
	void *_net_connector;


public:
	RpcServiceConnector(RpcSendFuncType sender, void *net_connector):_sender(sender),
				_net_connector(net_connector) {}
	virtual ~RpcServiceConnector();
	virtual void CallMethod(const MethodDescriptor *method, RpcController *controller,
			const Message *request, const Message *response, Closure *done);
	void HandleRpcResponse(unsigned char *response_data, size_t length);
};

