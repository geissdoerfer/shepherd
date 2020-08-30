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

#include "stdint.h"
#include "printf.h"
#include "stdint_optimized.h"

typedef void (*putcf) (void*, uint8_t);  // TODO: despite the datasheet char does not seem to be uint32_t on PRU, so it seems we need to go back

static void uint_to_ascii(uint32_t number, const uint32_t base, const bool_ft upper_case, uint8_t * buf)
	{
	uint32_t num = number;
    int32_t digit_count=0;
	uint32_t base_pwr=1;
	while (num/base_pwr >= base) // TODO: if numbers are transferred often, the division could be replaced by a LUT
        base_pwr*=base;
	while (base_pwr!=0) {
		const uint32_t digit = num / base_pwr;
        num -= base_pwr; // NOTE: replaced expensive modulo, not needed here
        base_pwr/=base;
		if (digit_count || digit>0 || base_pwr==0) {
			*buf++ = digit+(digit<10 ? '0' : (upper_case ? 'A' : 'a')-10);
			++digit_count;
			}
		}
	*buf=0;
	}

static void int_to_ascii(const int32_t number, uint8_t * buf)
	{
	uint32_t num;
    if (number<0) {
        num=(uint32_t)-number;
		*buf++ = '-';
		}
    else num = (uint32_t)number;
	uint_to_ascii(num, 10U, 0U, buf);
	}

static uint8_ft ascii_to_digit(const uint8_t character)
	{
	if (character>='0' && character<='9')
		return (character - '0');
	else if (character>='a' && character<='f')
		return (character - 'a' + 10);
	else if (character>='A' && character<='F')
		return (character - 'A' + 10);
	else return 255U;
	}

static uint8_t ascii_to_uint(const uint8_t character, const uint8_t**const src, const uint32_t base, uint32_t *const number_ptr)
	{
    uint8_t ch = character;
    const uint8_t* src_ptr= *src;
	uint32_t number=0;
	uint32_t digit;
	while ((digit = ascii_to_digit(ch)) < 255U) {
		if (digit > base) break;
        number=number*base+digit;
        ch=*src_ptr++;
		}
	*src=src_ptr;
	*number_ptr=number;
	return ch;
	}

// looks like "put character with left padding", padding is zero or spaces
static void put_chw(void* put_ptr, putcf put_fn, uint32_t ch_count, const uint8_t zero, const uint8_t * ch_buf)
	{
    uint8_t fill_char= zero ? '0' : ' ';
    uint8_t ch;
    const uint8_t* buf_ptr = ch_buf;
	while (*buf_ptr++ && ch_count > 0)
		ch_count--;
	while (ch_count-- > 0)
		put_fn(put_ptr,fill_char);
	while ((ch= *ch_buf++))
		put_fn(put_ptr,ch);
	}

void tfp_format(void* dst_ptr, putcf put_fn, const uint8_t *src_ptr, va_list va)
	{
    uint8_t buffer[12];
    uint8_t character;

	while ((character=*(src_ptr++)) > 0) {
		if (character!='%')
			put_fn(dst_ptr,character);
		else {
            uint8_t lzero=0;
			uint32_t w=0;   // TODO: there is something off. w is like width (ch_count) in put_chw, but is a real number coming from ascii_to_uint()
            character=*(src_ptr++);
			if (character=='0') {
                character=*(src_ptr++);
                lzero=1;
				}
			if (character>='0' && character<='9') {
                character= ascii_to_uint(character, &src_ptr, 10U, &w);
				}

			switch (character) {
				case 0:
					goto abort;
				case 'u' :
				    uint_to_ascii(va_arg(va, uint32_t), 10U, 0U, buffer);
                    put_chw(dst_ptr, put_fn, w, lzero, buffer);
					break;
				case 'd' :
				    int_to_ascii(va_arg(va, int32_t), buffer);
                    put_chw(dst_ptr, put_fn, w, lzero, buffer);
					break;
				case 'x': case 'X' :
				    uint_to_ascii(va_arg(va, uint32_t), 16U, (character=='X'), buffer);
                    put_chw(dst_ptr, put_fn, w, lzero, buffer);
					break;
				case 'c' :
					put_fn(dst_ptr,(uint8_t)(va_arg(va, int32_t)));
					break;
				case 's' : put_chw(dst_ptr, put_fn, w, 0U, va_arg(va, uint8_t*));
					break;
				case '%' :
					put_fn(dst_ptr,character);
				default:
					break;
				}
			}
		}
	abort:;
	}


void put_copy(void* dst_ptr, const uint8_t character)
	{
	*(*((char**)dst_ptr))++ = character;
	}



void tfp_sprintf(uint8_t* dst_ptr, uint8_t *src_ptr, ...)
	{
	va_list va;
	va_start(va,src_ptr);
        tfp_format(&dst_ptr, put_copy, src_ptr, va);
        put_copy(&dst_ptr, 0U);
	va_end(va);
	}
