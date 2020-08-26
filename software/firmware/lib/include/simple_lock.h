#ifndef __SIMPLE_LOCK_H_
#define __SIMPLE_LOCK_H_

#include <stdint.h>


typedef struct {
    unsigned int lock_pru0;
    unsigned int lock_pru1;
}__attribute__((packed)) simple_mutex_t;

inline void simple_mutex_enter(volatile simple_mutex_t * mutex);
inline void simple_mutex_exit(volatile simple_mutex_t * mutex);


#endif /* __SIMPLE_LOCK_H_ */
