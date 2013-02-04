
#include <stdio.h>
#include <stdlib.h>
#include "rpc_channel.h"

#include "helloworld.pb.h"
#include "rpc_controller.h"

using namespace PBRPC;

void FooDone(FooResponse *response, RpcController *controller) {
	if (controller->Failed()) {
		printf("test 2 Rpc Failed : %s\n", controller->ErrorText().c_str());
	} else {
		printf("++++++ test 2 Rpc Response is %s\n", response->text().c_str());
	}

	delete response;
	delete controller;
}

int main(int argc, char *argv[]) {
	RpcClient client;
	RpcChannel channel(&client, "127.0.0.1:18669");
	EchoService::Stub echo_clt(&channel);
	FooRequest request;
	request.set_text("test1");
	request.set_times(1);

	FooResponse response;
	RpcController controller;
	echo_clt.Foo(&controller, &request, &response, NULL);
	if (controller.Failed()) {
		printf("test 1 Rpc Call Failed : %s\n", controller.ErrorText().c_str());
	} else {
		printf("++++++ test 1 Rpc Response is %s\n", response.text().c_str());
	}

	request.set_text("test2");
	request.set_times(2);
	FooResponse *response2 = new FooResponse;
	RpcController *controller2 = new RpcController;
	echo_clt.Foo(controller2, &request, response2, google::protobuf::NewCallback(&FooDone, response2, controller2));
	
	request.set_text("test3");
	request.set_times(3);
	controller.Reset();
	RpcChannel channel2(&client, "127.0.0.1:18669");
	EchoService::Stub echo_clt2(&channel2);
	echo_clt2.Foo(&controller, &request, &response, NULL);

	if (controller.Failed()) {
		printf("test 3 Rpc Call Failed : %s\n", controller.ErrorText().c_str());
	} else {
		printf("++++++ test 3 Rpc Response is %s\n", response.text().c_str());
	}
	sleep(3);

	return 0;
}
