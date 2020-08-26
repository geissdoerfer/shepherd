
#ifndef __RPMSG_H_
#define __RPMSG_H_

#include <stdarg.h>
#include <stdint.h>
#include <pru_rpmsg.h>
#include "printf.h"

void rpmsg_putraw(void * data, unsigned int len);
void rpmsg_printf(unsigned char *fmt, ...);
void rpmsg_init(unsigned char * chan_name);
void rpmsg_flush();
int rpmsg_get(unsigned char * s);

#define printf rpmsg_printf

#endif /* __RPMSG_H_ */
