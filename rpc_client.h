
namespace PBRPC {
	class RpcChannel;
	class RpcClient {
		protected:
			int Connect(const char *con_str);
			void DisConnect(RpcChannel *channel);
			void DoRpc_syn(RpcChannel *channel, char *rpc_data, size_t length);
			void DoRpc_syn(RpcChannel *channel, char *rpc_data, size_t length);
	};
}