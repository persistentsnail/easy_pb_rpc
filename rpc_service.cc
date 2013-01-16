#include "rpc.pb.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>

using namespace ::google::protobuf;


class RpcServiceMgr
{
public:
	
	struct MethodData {
		const MethodDescriptor *_method_descriptor;
		const Message *_request_proto;
		const Message *_response_proto;
		Service *_rpc_service;
	};
	typedef int (*RpcSendFuncType)(char *data_buf, size_t length, void *caller);

private:
	MethodData _methods[MAX_RPC_SERVICEs][MAX_SERVICE_METHODs];
	RpcController _rpc_controller;
	RpcSendFuncType _rpc_data_sender;
	void * _cur_caller;

public:
	RpcServiceMgr(RpcSendFuncType rpc_data_sender) : _rpc_data_sender(rpc_data_sender) {}
	inline void AddMethod (unsigned int service_id, 
					unsigned int method_id,
					const MethodDescriptor *method_descriptor,
					const Message *request_proto,
					const Message *response_proto,
					Service *rpc_service) {
					MethodData *method_data = &_methods[service_id][method_id];
					method_data->_method_descriptor = method_descriptor;
					method_data->_request_proto = request_proto;
					method_data->_response_proto = response_proto;
					method_data->_rpc_service = rpc_service;
	}
	inline MethodData * GetMethod (unsigned int service_id, unsigned int method_id) {
		return &_methods[service_id][method_id];
	}

	inline RpcController *GetRpcController() { return _rpc_controller; }
	inline RpcSendFuncType GetRpcDataSender() { return _rpc_data_sender; } 

	inline void SetCurCaller(void *caller) { _cur_caller = caller; }
	inline void * GetCurCaller() { return _cur_caller; }
	
	static void RpcCallDone(Message *response, unsigned int call_id)
	{
		RPC::RpcResponseData rpc_data;
		std::string content, serialized_str;
		rpc_data.set_call_id(call_id);
		response->SerializeToString(&content);
		rpc_data.set_content(content); 
		rpc_data.SerializeToString(&serialized_str);
		_rpc_data_sender(serialized_str.c_str(), serialized_str.size(), _cur_caller);
	}
};

RpcServiceMgr * RpcServiceMgrInstance(RpcServiceMgr::RpcSendFuncType rpc_data_sender)
{
	static RpcServiceMgr mgr(rpc_data_sender);
	return &mgr;
}

void RegisterRpcService(RpcServiceMgr *mgr, Service *rpc_service, unsigned int service_id)
{
	const ServiceDescriptor *service_descriptor = rpc_service->GetDescriptor();
	for (int i = 0; i < service_descriptor->method_count(); i++) {
		const MethodDescriptor *method_descriptor = service_descriptor->method(i);	
		const Message *request_proto = &rpc_service->GetRequestPrototype(method_descriptor); 
		const Message *response_proto = &rpc_service->GetResponsePrototype(method_descriptor);
		mgr->AddMethod(service_id, i, method_descriptor,
					request_proto, response_proto, rpc_service);
	}
}

int HandleRpcCall(RpcServiceMgr *mgr, unsigned char *call_data, size_t length, void *caller)
{
	RpcController *rpc_controller = mgr->GetRpcController();
	RPC::RpcRequestData rpc_data;

	rpc_controller->Reset();
	mgr->SetCurCaller(caller);

	RpcServiceMgr::MethodData *the_method = mgr->GetMethod(rpc_data.service_id(),
			rpc_data.method_id());
	rpc_data.ParseFromArray(call_data, length);

	Message *request = the_method->_request_proto->New();
	Message *response = the_method->_response_proto->New();
	request->ParseFromString(rpc_data.content());

	the_method->_rpc_service->CallMethod(the_method->_method_descriptor,
										rpc_controller,
										request,
										response,
										NewCallback(
											mgr,
											&RpcServiceMgr::RpcCallDone, 
											response, 
											rpc_data.call_id()
											)
										);
	delete request;
	delete response;
	return 1;
}
