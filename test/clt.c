#include "rpc_channel.h"
#include "helloworld.pb.h"

using namespace PBRPC;

int main(int argc, char *argv[]) {
	RpcClient client;
	RpcChannel channel(&client, "127.0.0.1:18669");
	EchoService::Stub echo_clt(&channel);
	FooRequest request;
	request.set_text("longzhiri");
	request.set_times(100);

	FooResponse response;
	RpcController controller;
	echo_clt.Foo(&controller, &request, &response, NULL);

	return 0;
}
