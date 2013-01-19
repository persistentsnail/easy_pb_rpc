
#include <google/protobuf/service>

class RpcServiceMgr;

RpcServiceMgr * RpcServiceMgrInstance(RpcSendFuncType rpc_data_sender = NULL);
int RegisterRpcService(RpcServiceMgr *mgr, ::google::protobuf::Service *rpc_service, unsigned int service_id);

int HandleRpcCall(RpcServiceMgr *mgr, unsigned char *call_data, size_t length, void *caller);
