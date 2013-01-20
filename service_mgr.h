
#include <google/protobuf/service>

namespace PBRPC {

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
	ServiceData _services[MAX_RPC_SERVICEs];
	std::vector<unsigned int> _service_ids;
	RpcController _rpc_controller;

public:
	RpcServiceMgr() {}
	~RpcServiceMgr();

	bool AddMethod (unsigned int service_id, unsigned int method_id, const MethodDescriptor *method_descriptor, 
		const Message *request_proto, const Message *response_proto, Service *rpc_service);

	inline MethodData * GetMethod (unsigned int service_id, unsigned int method_id) {
		return &_services[service_id]._methods[method_id];
	}
	inline google::protobuf::Service * GetService (unsigned int service_id) {
		return _services[service_id]._rpc_service;
	}
	inline google::protobuf::RpcControl * GetRpcControl () {
		return _rpc_controller;
	}

	void HandleRpcCall(unsigned char *call_data, size_t length, std::string &ret_data);
	void RegisterRpcService(::google::protobuf::Service *rpc_service, unsigned int service_id);
};
}