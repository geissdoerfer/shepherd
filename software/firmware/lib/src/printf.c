/*
 * Copyright (c) 2004,2012 Kustaa Nyholm / SpareTimeLabs
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this list
 * of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * Neither the name of the Kustaa Nyholm or SpareTimeLabs nor the names of its
 * contributors may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#include "printf.h"

typedef void (*putcf) (void*, unsigned char);

static void ui2a(unsigned int num, unsigned int base, int uc, unsigned char * bf)
	{
	int n=0;
	unsigned int d=1;
	while (num/d >= base)
		d*=base;
	while (d!=0) {
		int dgt = num / d;
		num%= d;
		d/=base;
		if (n || dgt>0 || d==0) {
			*bf++ = dgt+(dgt<10 ? '0' : (uc ? 'A' : 'a')-10);
			++n;
			}
		}
	*bf=0;
	}

static void i2a (int num, unsigned char * bf)
	{
	if (num<0) {
		num=-num;
		*bf++ = '-';
		}
	ui2a(num,10,0,bf);
	}

static int a2d(unsigned char ch)
	{
	if (ch>='0' && ch<='9')
		return ch-'0';
	else if (ch>='a' && ch<='f')
		return ch-'a'+10;
	else if (ch>='A' && ch<='F')
		return ch-'A'+10;
	else return -1;
	}

static unsigned char a2i(unsigned char ch, unsigned char** src, int base, int* nump)
	{
	unsigned char* p= *src;
	int num=0;
	int digit;
	while ((digit=a2d(ch))>=0) {
		if (digit>base) break;
		num=num*base+digit;
		ch=*p++;
		}
	*src=p;
	*nump=num;
	return ch;
	}

static void putchw(void* putp, putcf putf, int n, unsigned char z, unsigned char* bf)
	{
    unsigned char fc=z? '0' : ' ';
    unsigned char ch;
    unsigned char* p=bf;
	while (*p++ && n > 0)
		n--;
	while (n-- > 0)
		putf(putp,fc);
	while ((ch= *bf++))
		putf(putp,ch);
	}

void tfp_format(void* putp, putcf putf, unsigned char *fmt, va_list va)
	{
    unsigned char bf[12];
    unsigned char ch;

	while ((ch=*(fmt++))) {
		if (ch!='%')
			putf(putp,ch);
		else {
			char lz=0;
			int w=0;
			ch=*(fmt++);
			if (ch=='0') {
				ch=*(fmt++);
				lz=1;
				}
			if (ch>='0' && ch<='9') {
				ch=a2i(ch,&fmt,10,&w);
				}

			switch (ch) {
				case 0:
					goto abort;
				case 'u' : {

					ui2a(va_arg(va, unsigned int),10,0,bf);
					putchw(putp,putf,w,lz,bf);
					break;
					}
				case 'd' :  {

					i2a(va_arg(va, int),bf);
					putchw(putp,putf,w,lz,bf);
					break;
					}
				case 'x': case 'X' :

					ui2a(va_arg(va, unsigned int),16,(ch=='X'),bf);
					putchw(putp,putf,w,lz,bf);
					break;
				case 'c' :
					putf(putp,(char)(va_arg(va, int)));
					break;
				case 's' :
					putchw(putp,putf,w,0,va_arg(va, char*));
					break;
				case '%' :
					putf(putp,ch);
				default:
					break;
				}
			}
		}
	abort:;
	}


void putcp(void* p, unsigned char c)
	{
	*(*((unsigned char**)p))++ = c;
	}



void tfp_sprintf(unsigned char* s, unsigned char *fmt, ...)
	{
	va_list va;
	va_start(va,fmt);
	tfp_format(&s,putcp,fmt,va);
	putcp(&s,0);
	va_end(va);
	}
