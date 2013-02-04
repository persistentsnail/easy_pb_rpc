
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include "common.h"

namespace PBRPC {

using google::protobuf::MethodDescriptor;
using google::protobuf::Message;
using google::protobuf::Service;

class RpcServiceMgr  {
public:

	struct MethodData {
		const MethodDescriptor *_method_descriptor;
		const Message *_request_proto;
		const Message *_response_proto;
	};

	struct ServiceData {
		google::protobuf::Service *_rpc_service;
		MethodData _methods[MAX_SERVICE_METHODs];
	};

private:
	ServiceData *_services;
	std::vector<unsigned int> _service_ids;

protected:
	bool AddMethod (unsigned int service_id, unsigned int method_id, const MethodDescriptor *method_descriptor, const Message *request_proto, const Message *response_proto, Service *rpc_service);

	inline MethodData * GetMethod (unsigned int service_id, unsigned int method_id) {
		return &_services[service_id]._methods[method_id];
	}
	inline google::protobuf::Service * GetService (unsigned int service_id) {
		return _services[service_id]._rpc_service;
	}

public:
	RpcServiceMgr() {_services = new ServiceData[MAX_RPC_SERVICEs];}
	~RpcServiceMgr();


	void HandleRpcCall(unsigned char *call_data, size_t length, std::string &ret_data, google::protobuf::RpcController *);
	bool RegisterRpcService(::google::protobuf::Service *rpc_service, unsigned int service_id);
};
}
