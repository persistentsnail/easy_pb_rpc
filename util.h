#include <pthread.h>
#include <unistd.h>

namespace PBRPC {
	struct SYNC {
		class Mutex {
			pthread_mutex_t _mutex;
		public:
			Mutex():_mutex(PTHREAD_MUTEX_INITIALIZER) {}
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
	};
}
