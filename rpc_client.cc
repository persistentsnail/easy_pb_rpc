#include <netinet/in.h> // sockaddr_in
#include <sys/socket.h> // socket functions
#include <arpa/inet.h> // htons
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sstream>

#include "rpc_client.h"
#include "rpc.pb.h"

using namespace PBRPC;

inline void RpcCallBack( google::protobuf::Closure *c, int wake_fd) {
	if (c) {
		c->Run();
	}else{
		char buf;
		write(wake_fd, &buf, sizeof(buf));
	}
}

inline void EventCb(struct bufferevent *bev, short events, void *arg) {
	Session *the_session = (Session *)arg;
	char err_str[1024] = "connection is closed";
	if (events & BEV_EVENT_CONNECTED) {
		return;
	} else if (!(events & BEV_EVENT_EOF)) {
		snprintf(err_str, sizeof(err_str), "read rpc sock failed with error %s", strerror(errno));
	}
	the_session->DisConnect(err_str);
}

inline void ReadCb(struct bufferevent *bev, void *ptr) {
	Session *the_session = (Session *)ptr;
	struct evbuffer *input = bufferevent_get_input(bev);
	the_session->OnCallBack(input);
}

inline void DoRpc(int notifier_read_handle, short event, void *arg) {
	char buf;
	if (read(notifier_read_handle, &buf, 1) <= 0) {
		ERR_LOG("RpcClient DoRpc : read notifier_read_handle failed");
		return;
	}
	RpcClient *this_clt = (RpcClient *)arg;
	MessageQueue::Node *msg = NULL;
	
	while (msg = this_clt->MessageDequeue()) {
		if (msg->_session_id <= 0 || msg->_session_id > RpcClient::MAX_SESSION_SIZE) {
			ERR_LOG("DoRpc Failed : Wrong session Id");
			return;
		}
		Session *the_session = this_clt->GetSession(msg->_session_id);
		if (msg->_kind == MessageQueue::CONNECT) {
			the_session->Connect(msg);
		} else if (msg->_kind == MessageQueue::CALL) {
			the_session->DoRpcCall(msg);
		}
		delete msg;
	}
}

/*
inline void RecycleSessionTimer(evutil_socket_t fd, short what, void *arg) {
	RpcClient *clt = (RpcClient *)arg;
	clt->DoRecycleSession();
}
*/

inline void * ThreadEntry(void *arg) {
	RpcClient *this_clt = (RpcClient *)arg;
	int notifier_read_handle = this_clt->GetNotifierReadHandle();
	struct event_base *evbase;
	evbase = event_base_new();
	this_clt->SetEventBase(evbase);
	struct event *listener = event_new(evbase, notifier_read_handle, EV_READ|EV_PERSIST,
		DoRpc, arg);
	event_add(listener, NULL);
	/*
	struct timeval interval = {5, 0};
	struct event *timer_event = event_new(evbase, -1, EV_PERSIST, RecycleSessionTimer, this_clt);
	event_add(timer_event, &interval);
	*/
	event_base_dispatch(evbase);
	this_clt->SetEventBase(NULL);
	event_base_free(evbase);
}

void Session::Connect(MessageQueue::Node *conn_msg) {
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr = conn_msg->_connect._ip;
	sin.sin_port = conn_msg->_connect._port;
	bufferevent *bev = bufferevent_socket_new(_owner_clt->_evbase, -1, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev, ReadCb, NULL, EventCb, this); 
	bufferevent_enable(bev, EV_READ | EV_WRITE);
	if (bufferevent_socket_connect(bev, (struct sockaddr *)&sin, sizeof(sin))<0) {
		bufferevent_free(bev);
		conn_msg->_controller->SetFailed("Connect Failed");
	} else 
		InitSession(bev);
}

void Session::DisConnect(const char *err) {
	bufferevent_free(_bev);
	_bev = NULL;
	TAllocator<TCallBack, SYNC::NullMutex>::Iterator iter = _calls.Begin();
	unsigned int call_id = 0;
	while (call_id = iter.Next()) {
		TCallBack *cb = GetCallBack(call_id);
		cb->_controller->SetFailed(err);
		RpcCallBack(cb->_c, cb->_wake_fd);
	}
}

void Session::DoRpcCall(MessageQueue::Node *call_msg) {
	unsigned int call_id = 0;
	if (!_bev) {
		call_msg->_controller->SetFailed("Session has disconnect");
		goto CALL_FINALLY;
	}

	if (!(call_id = AllocCallId())) {
		call_msg->_controller->SetFailed("Call id alloc failed");
		goto CALL_FINALLY;
	}
	//
	{
		TCallBack *cb = GetCallBack(call_id);
		cb->_controller = call_msg->_controller;
		cb->_response = call_msg->_call._cb._response;
		cb->_c = call_msg->_call._cb._c;
		cb->_wake_fd = call_msg->_call._cb._wake_fd;

		RPC::RpcRequestData rpc_data;

		rpc_data.set_call_id(call_id);
		rpc_data.set_service_id(call_msg->_call._service_id);
		rpc_data.set_method_id(call_msg->_call._method_id);
		rpc_data.set_content(*call_msg->_call._req_data);
		std::string data_buf;
		rpc_data.SerializeToString(&data_buf);
		LENGTH_TYPE header = data_buf.size();
		if (-1 == bufferevent_write(_bev, &header, HEAD_LEN) ||
			-1 == bufferevent_write(_bev, data_buf.c_str(), data_buf.size())) {
			call_msg->_controller->SetFailed("bufferevent_write Failed");
		}
	}
CALL_FINALLY:
	if (call_msg->_call._req_data)
		delete call_msg->_call._req_data;
	if (call_msg->_controller->Failed()) {
			// 出错立即回调
			RpcCallBack(call_msg->_call._cb._c, call_msg->_call._cb._wake_fd);
			if (call_id)
				FreeCallId(call_id);
	}
}

void Session::OnCallBack(struct evbuffer *input) {
	size_t buf_len;
	for (;;) {
		switch (_state) {
			case Session::ST_HEAD: {
				buf_len = evbuffer_get_length(input);
				if (buf_len < HEAD_LEN) return ;
				evbuffer_remove(input, &_data_length, HEAD_LEN);
				_state = Session::ST_DATA;
				break;
			}
			case Session::ST_DATA: {
				buf_len = evbuffer_get_length(input);
				if (buf_len < _data_length) return ;
				RPC::RpcResponseData rpc_data;
				rpc_data.ParseFromArray(evbuffer_pullup(input, _data_length), _data_length);
				evbuffer_drain(input, _data_length);
				Session::TCallBack *cb = GetCallBack(rpc_data.call_id());
				if (cb) {
					cb->_response->ParseFromString(rpc_data.content());
					RpcCallBack(cb->_c, cb->_wake_fd);
				}
				FreeCallId(rpc_data.call_id()); //TODO: Destroy Session
				_state = Session::ST_HEAD;
				break;
			}
			default: {
				ERR_LOG("Error Session state");
				break;
			}
		}
	}
}

RpcClient::RpcClient():_sessions(MAX_SESSION_SIZE),_is_start(false),_nrecycle_ss(0) {
}

RpcClient::~RpcClient() {
}

bool RpcClient::Start(::google::protobuf::RpcController *controller) {
	if (_is_start) return true; // Double check
	_start_mutex.Lock();
	if (!_is_start) {
		if (pipe(_notifier_pipe) != 0) {
			controller->SetFailed("Create Notifier pipe Failed");
			return false;
		}
		pthread_t tid;
		if (0 != pthread_create(&tid, NULL, ThreadEntry, this)) {
			controller->SetFailed("Create Thread Failed");
			return false;
		}
		_is_start = true;
	}
	_start_mutex.Unlock();
	return true;
}


bool RpcClient::CallMsgEnqueue(unsigned int session_id, std::string *req_data, 
	unsigned int service_id, unsigned int method_id,
	google::protobuf::RpcController *controller, google::protobuf::Message *response,
	google::protobuf::Closure *c, int wake_fd) {
	MessageQueue::Node *message = new MessageQueue::Node;
	message->_kind = MessageQueue::CALL;
	message->_session_id = session_id;
	message->_call._req_data = req_data;
	message->_call._service_id = service_id;
	message->_call._method_id = method_id;
	message->_controller = controller;
	message->_call._cb._response = response;
	message->_call._cb._c = c;
	message->_call._cb._wake_fd = wake_fd;
	bool success;
	_msg_queue_mutex.Lock();
	success = _msg_queue.Enqueue(message);
	_msg_queue_mutex.Unlock();
	if (!success) {
		delete message;
		controller->SetFailed("Call Message Enqueue Failed");
		return false;
	}
	char buf;
	if (write(_notifier_pipe[1], &buf, 1) <= 0) {
		controller->SetFailed("Write notifier_write_handle");
		return false;
	}
	return true;
}

bool RpcClient::ConnectMsgEnqueue(unsigned int session_id, 
									google::protobuf::RpcController *controller, 
									struct in_addr ip,
									unsigned short port) {

	MessageQueue::Node *message = new MessageQueue::Node;
	message->_kind = MessageQueue::CONNECT;
	message->_session_id = session_id;
	message->_controller = controller;
	message->_connect._ip = ip;
	message->_connect._port = port;
	bool success;
	_msg_queue_mutex.Lock();
	success = _msg_queue.Enqueue(message);
	_msg_queue_mutex.Unlock();
	if (!success) {
		delete message;
		ERR_LOG("Connect Msg Enqueue Failed");
		return false;
	}
	char buf;
	if (write(_notifier_pipe[1], &buf, 1) <= 0) {
		ERR_LOG("ConnectMsgEnqueue, Write notifier_write_handle");
		return false;
	}
	return true;
}

MessageQueue::Node *RpcClient::MessageDequeue() {
	MessageQueue::Node *node;
	_msg_queue_mutex.Lock();
	node = _msg_queue.Dequeue();
	_msg_queue_mutex.Unlock();
	return node;
}

