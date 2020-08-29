
#ifndef __RPMSG_H_
#define __RPMSG_H_

#include <stdarg.h>
#include <stdint.h>
#include <pru_rpmsg.h>
#include "printf.h"

void rpmsg_putraw(const void * data, uint32_t len);
void rpmsg_printf(const uint8_t *fmt, ...);
void rpmsg_init(const uint8_t * chan_name);
void rpmsg_flush();
int32_t rpmsg_get(uint8_t * buffer);

#define printf rpmsg_printf

#endif /* __RPMSG_H_ */
