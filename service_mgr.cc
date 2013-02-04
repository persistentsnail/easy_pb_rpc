#include "rpc.pb.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include "service_mgr.h"

using namespace ::google::protobuf;
using namespace PBRPC;


RpcServiceMgr::~RpcServiceMgr() {
	std::vector<unsigned int>::iterator iter;
	for (iter = _service_ids.begin(); iter != _service_ids.end(); iter++) {
		Service *rpc_service = _services[*iter]._rpc_service;
		delete rpc_service;
	}
	delete [] _services;
}

bool RpcServiceMgr::AddMethod (unsigned int service_id, unsigned int method_id, const MethodDescriptor *method_descriptor, 
	const Message *request_proto, const Message *response_proto, Service *rpc_service) {
		if (service_id < 0 || service_id >= MAX_RPC_SERVICEs || method_id < 0 || method_id >= MAX_SERVICE_METHODs) {
			ERR_LOG("Service Id or Method Id is INVALID");
			return false;
		}

		ServiceData *service_data = &_services[service_id];
		MethodData *method_data = &service_data->_methods[method_id];
		method_data->_method_descriptor = method_descriptor;
		method_data->_request_proto = request_proto;
		method_data->_response_proto = response_proto;
		service_data->_rpc_service = rpc_service;
		_service_ids.push_back(service_id);
		return true;
}

bool RpcServiceMgr::RegisterRpcService(Service *rpc_service, unsigned int service_id) {
	const ServiceDescriptor *service_descriptor = rpc_service->GetDescriptor();
	for (int i = 0; i < service_descriptor->method_count(); i++) {
		const MethodDescriptor *method_descriptor = service_descriptor->method(i);	
		const Message *request_proto = &rpc_service->GetRequestPrototype(method_descriptor); 
		const Message *response_proto = &rpc_service->GetResponsePrototype(method_descriptor);
		if (!AddMethod(service_id, i, method_descriptor,request_proto, 
								response_proto, rpc_service))
			return false;
	}
	return true;
}

void RpcServiceMgr::HandleRpcCall(unsigned char *call_data, size_t length, std::string &ret_data, google::protobuf::RpcController *controller) {
	RPC::RpcRequestData rpc_data;
	rpc_data.ParseFromArray(call_data, length);
	controller->Reset();

	RpcServiceMgr::MethodData *the_method = GetMethod(rpc_data.service_id(),
			rpc_data.method_id());
	Service *rpc_service = GetService(rpc_data.service_id());

	Message *request = the_method->_request_proto->New();
	Message *response = the_method->_response_proto->New();
	request->ParseFromString(rpc_data.content());

	rpc_service->CallMethod(the_method->_method_descriptor, controller, request, response, NULL);

	RPC::RpcResponseData response_data;
	std::string content;
	response_data.set_call_id(rpc_data.call_id());
	response->SerializeToString(&content);
	response_data.set_content(content); 
	response_data.SerializeToString(&ret_data);

	delete request;
	delete response;
}
