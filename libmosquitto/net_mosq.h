/*
Copyright (c) 2010,2011,2013 Roger Light <roger@atchoo.org>
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
#ifndef _NET_MOSQ_H_
#define _NET_MOSQ_H_

#ifndef WIN32
#include <unistd.h>
#else
#include <winsock2.h>
typedef int ssize_t;
#endif

#include "mosquitto_internal.h"
#include "mosquitto.h"

#ifdef WITH_BROKER
struct mosquitto_db;
#endif

#ifdef WIN32
#  define COMPAT_CLOSE(a) closesocket(a)
#  define COMPAT_ECONNRESET WSAECONNRESET
#  define COMPAT_EWOULDBLOCK WSAEWOULDBLOCK
#else
#  define COMPAT_CLOSE(a) close(a)
#  define COMPAT_ECONNRESET ECONNRESET
#  define COMPAT_EWOULDBLOCK EWOULDBLOCK
#endif

#ifndef WIN32
#else
#endif

/* For when not using winsock libraries. */
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

/* Macros for accessing the MSB and LSB of a uint16_t */
#define MOSQ_MSB(A) (uint8_t)((A & 0xFF00) >> 8)
#define MOSQ_LSB(A) (uint8_t)(A & 0x00FF)

void _mosquitto_net_init(void);
void _mosquitto_net_cleanup(void);

void _mosquitto_packet_cleanup(struct _mosquitto_packet *packet);
int _mosquitto_packet_queue(struct mosquitto *mosq, struct _mosquitto_packet *packet);
int _mosquitto_socket_connect(struct mosquitto *mosq, const char *host, uint16_t port, const char *bind_address, bool blocking);
int _mosquitto_socket_close(struct mosquitto *mosq);
int _mosquitto_try_connect(const char *host, uint16_t port, int *sock, const char *bind_address, bool blocking);

int _mosquitto_read_byte(struct _mosquitto_packet *packet, uint8_t *byte);
int _mosquitto_read_bytes(struct _mosquitto_packet *packet, void *bytes, uint32_t count);
int _mosquitto_read_string(struct _mosquitto_packet *packet, char **str);
int _mosquitto_read_uint16(struct _mosquitto_packet *packet, uint16_t *word);

void _mosquitto_write_byte(struct _mosquitto_packet *packet, uint8_t byte);
void _mosquitto_write_bytes(struct _mosquitto_packet *packet, const void *bytes, uint32_t count);
void _mosquitto_write_string(struct _mosquitto_packet *packet, const char *str, uint16_t length);
void _mosquitto_write_uint16(struct _mosquitto_packet *packet, uint16_t word);

ssize_t _mosquitto_net_read(struct mosquitto *mosq, void *buf, size_t count);
ssize_t _mosquitto_net_write(struct mosquitto *mosq, void *buf, size_t count);

int _mosquitto_packet_write(struct mosquitto *mosq);
#ifdef WITH_BROKER
int _mosquitto_packet_read(struct mosquitto_db *db, struct mosquitto *mosq);
#else
int _mosquitto_packet_read(struct mosquitto *mosq);
#endif

#ifdef WITH_TLS
int _mosquitto_socket_apply_tls(struct mosquitto *mosq);
#endif

#endif
