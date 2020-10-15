
#ifndef __RPMSG_H_
#define __RPMSG_H_

#include <stdarg.h>
#include <stdint.h>
#include <pru_rpmsg.h>
#include "printf.h"

/*
void rpmsg_putraw(const void * data, uint32_t len);  // TODO: use when pssp gets changed
void rpmsg_printf(const char *fmt, ...);
void rpmsg_init(const char * chan_name);
*/
void rpmsg_putraw(void * data, uint32_t len);
void rpmsg_printf(char *fmt, ...);
void rpmsg_init(char * chan_name);

void rpmsg_flush();
uint32_t rpmsg_get(uint8_t * buffer);

#define printf(x)   rpmsg_printf(x)

#endif /* __RPMSG_H_ */
