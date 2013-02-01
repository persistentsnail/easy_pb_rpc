
#include <string>
#include "helloworld.pb.h"
#include "rpc_server.h"

using namespace PBRPC;

class EchoServiceImpl : public EchoService {
	public:
	virtual void Foo(::google::protobuf::RpcController* controller,
                       const ::FooRequest* request,
                       ::FooResponse* response,
                       ::google::protobuf::Closure* done) {
		std::string str = request->text();
		for (int i = 1; i < request->times(); i++) {
			str += (" " + str);
		response->set_text(str);
		response->set_result(true);
	}
};
  


int main(int argc, char *argv[] ){
	RpcServer server;
	EchoServiceImpl *impl = new EchoServiceImpl();
	server.RegisterService(impl);
	server.Start();
	return 0;
}


