
#ifndef __RPMSG_H_
#define __RPMSG_H_

#include <stdarg.h>
#include <stdint.h>
#include <pru_rpmsg.h>
#include "printf.h"

void rpmsg_putraw(void * data, uint32_t len);
void rpmsg_printf(uint8_t *fmt, ...);
void rpmsg_init(uint8_t * chan_name);
void rpmsg_flush();
int32_t rpmsg_get(uint8_t * s);

#define printf rpmsg_printf

#endif /* __RPMSG_H_ */
