
#include <google/protobuf/service.h>
#include <string>
#include "rpc_client.h"
#include "common.h"

namespace PBRPC {
	class RpcChannel : public google::protobuf::RpcChannel {
		friend RpcClient;
		class CallMgr {
			enum {MAX_CALLs_SIZE = 2 << 10};
			unsigned int *_unused_ids;
			unsigned int _unused_size;

			struct CallData {
				unsigned int _callId;
				struct {
					Closure *_c;
					Message *_arg;
				} _cb_data;
			};
			CallData *_calls;
		public:

			CallMgr() {
				_unused_ids = new unsigned int[MAX_CALLs_SIZE];
				_calls = new CallData[MAX_CALLs_SIZE];
				for (unsigned int i = 0; i < MAX_CALLs_SIZE; i++) {
					_unused_ids[i] = i;
					_calls[i]._callId = i;
				}
				_unused_size = MAX_CALLs_SIZE;
			}
			~CallMgr() { delete [] _unused_ids; delete [] _calls;}
			inline CallData * Alloc() {
				if (_unused_size <= 0)
					return NULL;
				return &_calls[_unused_ids[--_unused_size]];
			}
			inline void Free(unsigned int id) {
				assert(_unused_size < MAX_CALLs_SIZE);
				_unused_ids[_unused_size++] = id;
			}
			inline CallData * Get(unsigned int id) {
				if (id < 0 || id >= MAX_CALLs_SIZE)
					return NULL;
				return &_calls[id];
			}
		};

	private:
		RpcClient *_client;
		void * _handle;
		string _conn_str;
		CallMgr _call_mgr;
	public:
		RpcChannel(RpcClient *client, const char *connect_str);
		virtual ~RpcChannel() £û£ý
		virtual void CallMethod(const MethodDescriptor *method, RpcController *controller,
				const Message *request, const Message *response, Closure *done);
		void HandleRpcResponse(unsigned char *response_data, size_t length);

		inline void SetHandle(void * handle) { _handle = handle; }
		inline void GetHandle() { return _handle; }
	protected:
		void DisConnect();
	};
}
