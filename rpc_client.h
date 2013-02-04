
#include <string>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <google/protobuf/service.h>
#include "util.h"
#include "common.h"


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

				Node *_next, *_prev;
			};
	private:
			Node *_head, *_tail;

	public:
			MessageQueue():_head(NULL), _tail(NULL) {}
			bool Enqueue(Node *message) {
				if (!_head) {
					_head = _tail = message;
					message->_next = NULL;
					message->_prev = NULL;
				} else {
					message->_next = _head;
					_head->_prev = message;
					_head = message;
					message->_prev = NULL;
				}
				return true;
			}
					
			Node * Dequeue() {
				Node *last = NULL;
				if (!_head)
					return NULL;
				else {
					last = _tail;
					_tail = _tail->_prev;
					if (!_tail) _head = NULL;
					else _tail->_next = NULL;
				}
				return last;
			}
	};

	class RpcClient;

	class Session {
	public:

		struct TCallBack {
			google::protobuf::RpcController *_controller;
			google::protobuf::Message *_response;
			google::protobuf::Closure *_c;
			int _wake_fd;
		};
	private:

		TAllocator<TCallBack, SYNC::NullMutex> _calls;

		struct bufferevent *_bev;
		enum STATE {ST_HEAD, ST_DATA};
		LENGTH_TYPE _data_length;
		STATE _state;

		unsigned int _ncalls;

		RpcClient *_owner_clt;

	protected:
		inline unsigned int AllocCallId() {
			unsigned int id = _calls.Alloc();
			if (id) _ncalls++;
			return id;
		}
		inline void FreeCallId(unsigned int call_id) {
			_ncalls--;
			_calls.Free(call_id);
		}
		TCallBack *GetCallBack(unsigned int call_id) {
			return _calls.Get(call_id);
		}
		inline void InitSession(struct bufferevent *bev) {
			_bev = bev;
			_state = ST_HEAD;
			_data_length = 0;
			_ncalls = 0;
		}

	public:
		Session():_calls(256) {}
		void Connect(MessageQueue::Node *conn_msg);
		void DisConnect(const char *err);
		void DoRpcCall(MessageQueue::Node *call_msg);
		void OnCallBack(struct evbuffer *input);

		bool CanDestroy() {
			return !_bev || !_ncalls;
		}

		void SetOwner(RpcClient *owner) {
			_owner_clt = owner;
		}
	};

	class RpcClient {
		friend class Session;
		public:
			enum {MAX_SESSION_SIZE = 1024};
		private:
			struct event_base *_evbase;	

			MessageQueue _msg_queue;
			SYNC::Mutex _msg_queue_mutex;

			TAllocator<Session, SYNC::NullMutex> _sessions;
			unsigned int _recycle_sessions[MAX_SESSION_SIZE];
			unsigned int _nrecycle_ss;

			int _notifier_pipe[2];

			bool _is_start;
			SYNC::Mutex _start_mutex;
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
				unsigned int id = _sessions.Alloc();
				if (id == 0) {
					DoRecycleSession();
					id = _sessions.Alloc();
				}
				Session * s = GetSession(id);
				if (s) s->SetOwner(this);
				return id;
			}
			void FreeSession(unsigned int session_id) {
				assert(_nrecycle_ss < MAX_SESSION_SIZE);
				_recycle_sessions[_nrecycle_ss++] = session_id;
			}
			Session *GetSession(unsigned int session_id) {
				return _sessions.Get(session_id);
			}

			void DoRecycleSession() {
				for (unsigned int i = 0; i < _nrecycle_ss;) {
					unsigned int id = _recycle_sessions[i];
					Session *checked_one = GetSession(i);
					if (checked_one && checked_one->CanDestroy()) {
						_sessions.Free(id);
						if (--_nrecycle_ss > 0)
							_recycle_sessions[i] = _recycle_sessions[_nrecycle_ss];
					} else i++;
				}
			}

			inline int GetNotifierReadHandle() { return _notifier_pipe[0]; }
			inline void SetEventBase(struct event_base *base) { _evbase = base; }
			inline struct event_base *GetEventBase() { return _evbase; }
	};
}
