/*
Copyright (c) 2009-2013 Roger Light <roger@atchoo.org>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of mosquitto nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "memory_mosq.h"

#ifdef REAL_WITH_MEMORY_TRACKING
#  if defined(__APPLE__)
#    include <malloc/malloc.h>
#    define malloc_usable_size malloc_size
#  elif defined(__FreeBSD__)
#    include <malloc_np.h>
#  else
#    include <malloc.h>
#  endif
#endif

#ifdef REAL_WITH_MEMORY_TRACKING
static unsigned long memcount = 0;
static unsigned long max_memcount = 0;
#endif

void *_mosquitto_calloc(size_t nmemb, size_t size)
{
	void *mem = calloc(nmemb, size);

#ifdef REAL_WITH_MEMORY_TRACKING
	memcount += malloc_usable_size(mem);
	if(memcount > max_memcount){
		max_memcount = memcount;
	}
#endif

	return mem;
}

void _mosquitto_free(void *mem)
{
#ifdef REAL_WITH_MEMORY_TRACKING
	memcount -= malloc_usable_size(mem);
#endif
	free(mem);
}

void *_mosquitto_malloc(size_t size)
{
	void *mem = malloc(size);

#ifdef REAL_WITH_MEMORY_TRACKING
	memcount += malloc_usable_size(mem);
	if(memcount > max_memcount){
		max_memcount = memcount;
	}
#endif

	return mem;
}

#ifdef REAL_WITH_MEMORY_TRACKING
unsigned long _mosquitto_memory_used(void)
{
	return memcount;
}

unsigned long _mosquitto_max_memory_used(void)
{
	return max_memcount;
}
#endif

void *_mosquitto_realloc(void *ptr, size_t size)
{
	void *mem;
#ifdef REAL_WITH_MEMORY_TRACKING
	if(ptr){
		memcount -= malloc_usable_size(ptr);
	}
#endif
	mem = realloc(ptr, size);

#ifdef REAL_WITH_MEMORY_TRACKING
	memcount += malloc_usable_size(mem);
	if(memcount > max_memcount){
		max_memcount = memcount;
	}
#endif

	return mem;
}

char *_mosquitto_strdup(const char *s)
{
	char *str = strdup(s);

#ifdef REAL_WITH_MEMORY_TRACKING
	memcount += malloc_usable_size(str);
	if(memcount > max_memcount){
		max_memcount = memcount;
	}
#endif

	return str;
}

