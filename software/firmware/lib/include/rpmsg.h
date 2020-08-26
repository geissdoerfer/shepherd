
#ifndef __RPMSG_H_
#define __RPMSG_H_

#include <stdarg.h>
#include <stdint.h>
#include <pru_rpmsg.h>
#include "printf.h"

void rpmsg_putraw(void * data, unsigned int len);
void rpmsg_printf(char *fmt, ...);
void rpmsg_init(char * chan_name);
void rpmsg_flush();
int rpmsg_get(char * s);

#define printf rpmsg_printf

#endif /* __RPMSG_H_ */
