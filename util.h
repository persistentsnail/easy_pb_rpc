#include <pthread.h>
#include <unistd.h>
#include <algorithm>
#include <assert.h>

namespace PBRPC {
	struct SYNC {
		class Mutex {
			pthread_mutex_t _mutex;
		public:
			Mutex() {
				pthread_mutex_init(&_mutex, NULL);	
			}
			~Mutex() {
				pthread_mutex_destroy(&_mutex);
			}
			inline void Lock() {
				pthread_mutex_lock(&_mutex);
			}
			inline void Unlock() {
				pthread_mutex_unlock(&_mutex);
			}
		};
		class NullMutex {
		public:
			inline void Lock() {}
			inline void Unlock() {}
		};
		
		class Pipe {
		};
	};

	template <typename T, typename MUTEX_TRAITS>
	class TAllocator {
	public:
		friend class Iterator;
		class Iterator {
			unsigned int *_order_unused_ids;
			unsigned int _unused_size;
			unsigned int _high_unused_index;
			unsigned int _cur_id;
			unsigned int _max_id;
			public:
			Iterator(TAllocator<T, MUTEX_TRAITS> *ta) {
				_order_unused_ids = ta->_unused_ids;
				_unused_size = ta->_unused_size;
				std::sort(_order_unused_ids, _order_unused_ids + _unused_size);
				_high_unused_index = 0;
				_cur_id = 1;
				_max_id = ta->_total_size;
			}
			Iterator():_order_unused_ids(NULL), _unused_size(0),
				_high_unused_index(0), _cur_id(0), _max_id(0) {}

			unsigned int Next() {
				do { 
					if (_high_unused_index >= _unused_size)
						return _cur_id > _max_id ? 0 : _cur_id++;


					if (_cur_id < _order_unused_ids[_high_unused_index])
						return _cur_id++;
					else if (_cur_id == _order_unused_ids[_high_unused_index]) {
						_cur_id++;
						_high_unused_index++;
					} else assert(0);
				} while(1);
			}
		};
	private:
		unsigned int _total_size;
		T *_all_elments;

		unsigned int *_unused_ids;
		unsigned int _unused_size;
		MUTEX_TRAITS _mutex;
		
	public:
		TAllocator(unsigned int size = 1024):_total_size(size),_unused_size(size) {
			_all_elments = new T[_total_size + 1];
			_unused_ids = new unsigned int[_unused_size];
			for (unsigned int i = 0; i < _unused_size; i++) {
				_unused_ids[i] = i + 1;	
			}
		}
		~TAllocator() {
			delete [] _all_elments;
			delete [] _unused_ids;
		}
		unsigned int Alloc() {
			_mutex.Lock();
			if (_unused_size <= 0) return 0;
			unsigned int id = _unused_ids[--_unused_size];
			_mutex.Unlock();
			return id;
		}
		void Free(unsigned int id) {
			if (id < 1 || id > _total_size) return;
			_mutex.Lock();
			assert(_unused_size < _total_size);
			_unused_ids[_unused_size++] = id;
			_mutex.Unlock();
		}
		T *Get(unsigned int id) {
			if (id < 1 || id > _total_size) return NULL;
			return &_all_elments[id];
		}
		Iterator Begin() {
			return Iterator(this);
		}

	};
}
