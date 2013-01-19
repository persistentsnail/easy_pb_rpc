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
	};

	struct ServiceData {
		Service *_rpc_service;
		MethodData _methods[MAX_SERVICE_METHODs];
	};

private:
	ServiceData _services[MAX_RPC_SERVICEs];
	std::vector<unsigned int> _service_ids;
	RpcController _rpc_controller;
	RpcSendFuncType _rpc_data_sender;
	void * _cur_caller;

public:
	RpcServiceMgr(RpcSendFuncType rpc_data_sender) : _rpc_data_sender(rpc_data_sender) {}
	~RpcServiceMgr() {
		std::vector<unsigned int>::iterator iter;
		for (iter = _service_ids.begin(); iter != _service_ids.end(); iter++) {
			Service *rpc_service = _services[*iter]->_rpc_service;
			delete rpc_service;
		}
	}

	inline int AddMethod (unsigned int service_id, unsigned int method_id, const MethodDescriptor *method_descriptor, 
									const Message *request_proto, const Message *response_proto, Service *rpc_service) {
		if (service_id < 0 || service_id >= MAX_RPC_SERVICEs || method_id < 0 || method_id >= MAX_SERVICE_METHODs) {
			ERR_LOG("Service Id or Method Id is INVALID");
			return -1;
		}

		ServiceData *service_data = &_services[service_id];
		MethodData *method_data = &service_data->_methods[method_id];
		method_data->_method_descriptor = method_descriptor;
		method_data->_request_proto = request_proto;
		method_data->_response_proto = response_proto;
		service_data->_rpc_service = rpc_service;
		_service_ids.push_back(service_id);
		return 0;
	}

	inline MethodData * GetMethod (unsigned int service_id, unsigned int method_id) {
		return &_services[service_id]._methods[method_id];
	}
	inline Service * GetService (unsigned int service_id) {
		return _services[service_id]._rpc_service;
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

RpcServiceMgr * RpcServiceMgrInstance(RpcSendFuncType rpc_data_sender)
{
	static RpcServiceMgr mgr(rpc_data_sender);
	return &mgr;
}

int RegisterRpcService(RpcServiceMgr *mgr, Service *rpc_service, unsigned int service_id)
{
	const ServiceDescriptor *service_descriptor = rpc_service->GetDescriptor();
	for (int i = 0; i < service_descriptor->method_count(); i++) {
		const MethodDescriptor *method_descriptor = service_descriptor->method(i);	
		const Message *request_proto = &rpc_service->GetRequestPrototype(method_descriptor); 
		const Message *response_proto = &rpc_service->GetResponsePrototype(method_descriptor);
		if (mgr->AddMethod(service_id, i, method_descriptor,request_proto, 
								response_proto, rpc_service) != 0)
			return -1;
	}
	return 0;
}

int HandleRpcCall(RpcServiceMgr *mgr, unsigned char *call_data, size_t length, void *caller)
{
	RpcController *rpc_controller = mgr->GetRpcController();
	RPC::RpcRequestData rpc_data;

	rpc_controller->Reset();
	mgr->SetCurCaller(caller);

	RpcServiceMgr::MethodData *the_method = mgr->GetMethod(rpc_data.service_id(),
			rpc_data.method_id());
	Service *rpc_service = mgr->GetService(rpc_data.service_id());
	rpc_data.ParseFromArray(call_data, length);

	Message *request = the_method->_request_proto->New();
	Message *response = the_method->_response_proto->New();
	request->ParseFromString(rpc_data.content());

	rpc_service->CallMethod(the_method->_method_descriptor, rpc_controller, request, response,
								NewCallback(mgr, &RpcServiceMgr::RpcCallDone, response, rpc_data.call_id()));
	delete request;
	delete response;
	return 1;
}
