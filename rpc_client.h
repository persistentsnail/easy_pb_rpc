
#include <google/protobuf/service.h>
#include <string>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

namespace PBRPC {

	class MessageQueue {
	public:
			enum {CONNECT, CALL};
			struct Node {
				unsigned char _kind;
				unsigned int _session_id;
				google::protobuf::RpcController *_controller;

				union {
					struct {
						std::string *_req_data;
						unsigned int _service_id;
						unsigned int _method_id;
						struct {
							google::protobuf::Message *_response;
							google::protobuf::Closure *_c;
							int _wake_fd;
						}_cb;
					}_call;

					struct {
						struct in_addr _ip;
						unsigned short _port;
					}_connect;
				};

				Node *_next;
			};
	private:
			Node *_head, *_tail;

	public:
			MessageQueue():_head(NULL), _tail(NULL) {}
			bool Enqueue(Node *message);
					
			Node * Dequeue();
	};

	class Session {
		TAllocator<TCallBack, SYNC::NullMutex> _calls;
		struct bufferevent *_bev;
		enum STATE {ST_HEAD, ST_DATA};
		LENGTH_TYPE _data_length;
		STATE _state;

	public:

		struct TCallBack {
			google::protobuf::RpcController *_controller;
			google::protobuf::Message *_response;
			google::protobuf::Closure *_c;
			int _wake_fd;
		};
		Session():_calls(256) {}
	protected:
		inline unsigned int AllocCallId() {
			return _calls.Alloc();
		}
		inline void FreeCallId(unsigned int call_id) {
			_calls.Free(call_id);
		}
		TCallBack *GetCallBack(unsigned int call_id) {
			return _calls.Get(call_id);
		}
		inline void InitSession(struct bufferevent *bev) {
			_bev = bev;
			_state = ST_HEAD;
			_data_length = 0;
		}
	public:
		void Connect(MessageQueue::Node *conn_msg);
		void DoRpcCall(MessageQueue::Node *call_msg);
		void OnCallBack(struct evbuffer *input);
	};

	class RpcClient {
			struct event_base *_evbase;	

			MessageQueue _msg_queue;
			SYNC::Mutex _msg_queue_mutex;

			TAllocator<Session, SYNC::NullMutex> _sessions;
			enum {MAX_SESSION_SIZE = 1024};

			int _notifier_pipe[2];

			bool _is_start;
			SYNC::Mutext _start_mutex;
		public:
			RpcClient();
			~RpcClient();

			bool Start(google::protobuf::RpcController *);
			bool CallMsgEnqueue(unsigned int session_id, std::string *req_data, 
									unsigned int sevice_id, unsigned int method_id,
									google::protobuf::RpcController *controller, 
									google::protobuf::Message *response, 
									google::protobuf::Closure *c, int wake_fd
									);
			bool ConnectMsgEnqueue(unsigned int session_id, 
									google::protobuf::RpcController *controller,
									struct in_addr ip, unsigned short port
								  );
			MessageQueue::Node *MessageDequeue();
			unsigned int AllocSession() {
				return _sessions.Alloc();
			}
			void FreeSession(unsigned int session_id) {
				_session.Free(session_id);
			}
			Session *GetSession(unsigned int session_id) {
				return _sessions.Get(session_id);
			}

			inline int GetNotifierReadHandle() { return _notifier_pipe[0]; }
			inline void SetEventBase(struct event_base *base) { _evbase = base; }
			inline struct event_base *GetEventBase() { return _evbase; }

	};
}
