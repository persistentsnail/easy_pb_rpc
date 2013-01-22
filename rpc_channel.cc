#include <string>
#include <iostream>
#include <map>
#include "service_connector.h"

using namespace PBRPC;

void RpcChannel::CallMethod(const MethodDescriptor *method,	::google::protobuf::RpcController *controller,
								const Message *request, Message *response,	Closure *done) {
	if (!_handle)
		_handle = _client->Connect(_conn_str, controller);
	if (!_handle) return;

	RPC:RpcRequestData rpc_data;
	const string &service_name = method->service()->name();
	unsigned int service_id = RpcServiceName2Id(service_name);
	if (service_id == INVALID_SERVICE_ID) {
		controller->SetFailed("The Service Not Support!");
		return;
	}
	rpc_data->set_service_id(service_id);
	rpc_data->set_method_id(method->index());
	CallData *the_call = _call_mgr.Alloc();
	if (!the_call) {
		controller->SetFailed("Alloc Call  Failed!");
		return;
	}
	rpc_data->set_call_id(the_call->_callId);
	std::string content;
	request->SerializeToString(&content);
	rpc_data->set_content(content);
	std::string serialized_str; 
	rpc_data->SerializeToString(&serialized_str);
	
	the_call->_cb_data._c = done;
	the_call->_cb_data._arg = response;
	_client->DoRpc(this, serialized_str.c_str(), serialized_str.size());
	if (done == NULL) // synchronous
		_client->Wait();
}


void RpcChannel::HandleRpcResponse(unsigned char *response_data, size_t length) {
	RPC::RpcResponseData rpc_data;
	rpc_data.ParseFromArray(response_data, length);
	unsigned int call_id = rpc_data.call_id();
	CallData *the_call;
	if (!(the_call = _call_mgr.Get(call_id))) {
		ERR_LOG("HandleRpcResponse Failed : No Callback");
		return;
	}
	Closure *c = the_call->_cb_data._c;
	Message *response = the_call->_cb_data._arg;
	response->ParseFromString(rpc_data.content());
	if (c)	c->Run();
	_call_mgr.Free(call_id);
}

RpcChannel::RpcChannel(RpcClient *client, const char *connect_str):_client(client),
	_conn_str(connect_str), _handle(NULL) {
}

RpcChannel::~RpcChannel() {
	DisConnect();
}
