#include <string>
#include <iostream>
#include <map>
#include "service_connector.h"

using namespace google::protobuf;

class CallIdMgr
{
	unsigned int _int_ptr;
	unsigned int _out_ptr;
	enum {MAX_IDs_SIZE = 2 << 30};
	unsigned int *_ids;


public:
	static CallIdMgr * Instance() {
		static CallIdMgr IdMgr;
		return &IdMgr;
	}

	CallIdMgr():_int_ptr(0), _out_ptr(MAX_IDs_SIZE - 1) {
		_ids = new unsigned int[MAX_IDs_SIZE];
		for (unsigned int i = 0; i < MAX_IDs_SIZE; i++)
			_ids[i] = i;
	}
	~CallIdMgr() { delete [] _ids; }
	inline unsigned int Alloc(bool &success) {
		success = true;
		if (_int_ptr == _out_ptr) return success = false;
		unsigned int ret = _ids[_int_ptr];
		_int_ptr = (_int_ptr + 1) % MAX_IDs_SIZE;
		return ret;
	}
	inline void Free(unsigned int id) {
		assert((_out_ptr + 1) % MAX_IDs_SIZE != _int_ptr);
		_ids[_out_ptr] = id;
		_out_ptr = (_out_ptr + 1) % MAX_IDs_SIZE;
	}
};

class CallBackMgr {
	struct CallBackData {
		Closure *_c;
		Message *_arg;
	};
	std::map<unsigned int, CallBackData> _idtocb;
	typedef std::map<unsigned int, CallBackData> ID2CBT;
	typedef std::map<unsigned int, CallBackData> ID2CBT_ITER;
public:
	static CallBackMgr *Instance() {
		static CallBackMgr CBMgr;
		return &CBMgr;
	}
	void Put(unsigned int call_id, Closure *c, Message *arg) {
		CallBackData cbdata;
		cbdata._c = c;
		cbdata._arg = arg;
		_idtocb[call_id] = cbdata;
	}
	bool Get(unsigned int call_id, Closure *&c, Message *&arg) {
		ID2CBT_ITER iter = _idtocb.find(call_id);	
		if (iter == _idtocb.end()) return false;
		c = iter->second._c;
		arg = iter->second._arg;
		return true;
	}
	void Del(unsigned int call_id) {
		_idtocb.erase(call_id);
	}
};

void RpcServiceConnector::CallMethod(const MethodDescriptor *method,
											RpcController *controller, 
											const Message *request, 
											Message *response,
											Closure *done)
{
	RPC:RpcRequestData rpc_data;
	const string &service_name = method->service()->name();
	unsigned int service_id = RpcServiceName2Id(service_name);
	if (service_id == INVALID_SERVICE_ID) {
		controller->SetFailed("The Service Not Support!");
		return;
	}
	rpc_data->set_service_id(service_id);
	rpc_data->set_method_id(method->index());
	bool success;
	unsigned int call_id = CallIdMgr::Instance()->Alloc(success);
	if (!success) {
		controller->SetFailed("Alloc Call Id Failed!");
		return;
	}
	rpc_data->set_call_id(call_id);
	std::string content;
	request->SerializeToString(&content);
	rpc_data->set_content(content);
	std::string serialized_str; 
	rpc_data->SerializeToString(&serialized_str);
	_sender(serialized_str.c_str(), serialized_str.size(), _net_connector);
	
	CallBackMgr::Instance()->Put(call_id, c, response);
	if (done == NULL) // synchronous
}


void RpcServiceConnector::HandleRpcResponse(unsigned char *response_data, size_t length) {
	RPC::RpcResponseData rpc_data;
	rpc_data.ParseFromArray(response_data, length);
	unsigned int call_id = rpc_data.call_id();
	Closure *c;
	Message *response;
	if (!CallBackMgr::Instance()->Get(call_id, c, response)) {
		ERR_LOG("HandleRpcResponse Failed : No Callback");
		return;
	}
	response->ParseFromString(rpc_data.content());
	c->Run();
	CallBackMgr::Instance()->Del(call_id);
	CallIdMgr::Instance()->Free(call_id);
}
