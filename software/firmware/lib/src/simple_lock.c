#include "simple_lock.h"

#if defined PRU0
	#define MY_LOCK     lock_pru0
	#define OTHER_LOCK  lock_pru1
#elif defined PRU1
	#define MY_LOCK     lock_pru1
	#define OTHER_LOCK  lock_pru0
#else
	#error "PRU number must be defined"
#endif

inline void simple_mutex_enter(volatile simple_mutex_t *const mutex)
{
	/* Spin, if lock is taken by other PRU */
	while(mutex->OTHER_LOCK){};
	mutex->MY_LOCK = 1U;

	/* PRU1 has lower priority and will back up in case of simultaneous access */
	#ifdef PRU1
		/* Wait a little to guarantee detection of simultaneous access */
		__delay_cycles(5);
		while(mutex->OTHER_LOCK){};
	#endif

}

inline void simple_mutex_exit(volatile simple_mutex_t *const mutex)
{
	mutex->MY_LOCK = 0U;
}
