#ifndef POOL_H_INCLUDED
#define POOL_H_INCLUDED

struct slot_struct {
	// always lock the pool mutex
	// when reading OR writing the status
	volatile int status;
	// once you flag the slot with the STATUS_BUSY,
	// unlock the mutex and you are
	// free to mess with this stuff below
	char* name; // script filename
	time_t load; // timestamp loaded
	pthread_t tid; // thread using the state
	lua_State* state;
} typedef slot_t;

struct pool_struct {
	int count;
	pthread_mutex_t mutex;
	slot_t* slot;
} typedef pool_t;

pool_t* pool_open(int count);
void pool_close(pool_t* pool);

void slot_load(slot_t *p, lua_State* L, char* name);
void slot_flush(slot_t* p);

#endif // POOL_H_INCLUDED