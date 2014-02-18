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

#include <assert.h>
#include <string.h>

#include "mosquitto.h"
#include "logging_mosq.h"
#include "memory_mosq.h"
#include "mqtt3_protocol.h"
#include "net_mosq.h"
#include "send_mosq.h"
#include "util_mosq.h"

#ifdef WITH_BROKER
#include "mosquitto_broker.h"
#endif

int _mosquitto_send_connect(struct mosquitto *mosq, uint16_t keepalive, bool clean_session)
{
	struct _mosquitto_packet *packet = NULL;
	int payloadlen;
	uint8_t will = 0;
	uint8_t byte;
	int rc;
	uint8_t version = PROTOCOL_VERSION;

	assert(mosq);
	assert(mosq->id);

	packet = _mosquitto_calloc(1, sizeof(struct _mosquitto_packet));
	if(!packet) return MOSQ_ERR_NOMEM;

	payloadlen = 2+strlen(mosq->id);
	if(mosq->will){
		will = 1;
		assert(mosq->will->topic);

		payloadlen += 2+strlen(mosq->will->topic) + 2+mosq->will->payloadlen;
	}
	if(mosq->username){
		payloadlen += 2+strlen(mosq->username);
		if(mosq->password){
			payloadlen += 2+strlen(mosq->password);
		}
	}

	packet->command = CONNECT;
	packet->remaining_length = 12+payloadlen;
	rc = _mosquitto_packet_alloc(packet);
	if(rc){
		_mosquitto_free(packet);
		return rc;
	}

	/* Variable header */
	_mosquitto_write_string(packet, PROTOCOL_NAME, strlen(PROTOCOL_NAME));
#if defined(WITH_BROKER) && defined(WITH_BRIDGE)
	if(mosq->bridge && mosq->bridge->try_private && mosq->bridge->try_private_accepted){
		version |= 0x80;
	}else{
	}
#endif
	_mosquitto_write_byte(packet, version);
	byte = (clean_session&0x1)<<1;
	if(will){
		byte = byte | ((mosq->will->retain&0x1)<<5) | ((mosq->will->qos&0x3)<<3) | ((will&0x1)<<2);
	}
	if(mosq->username){
		byte = byte | 0x1<<7;
		if(mosq->password){
			byte = byte | 0x1<<6;
		}
	}
	_mosquitto_write_byte(packet, byte);
	_mosquitto_write_uint16(packet, keepalive);

	/* Payload */
	_mosquitto_write_string(packet, mosq->id, strlen(mosq->id));
	if(will){
		_mosquitto_write_string(packet, mosq->will->topic, strlen(mosq->will->topic));
		_mosquitto_write_string(packet, (const char *)mosq->will->payload, mosq->will->payloadlen);
	}
	if(mosq->username){
		_mosquitto_write_string(packet, mosq->username, strlen(mosq->username));
		if(mosq->password){
			_mosquitto_write_string(packet, mosq->password, strlen(mosq->password));
		}
	}

	mosq->keepalive = keepalive;
#ifdef WITH_BROKER
# ifdef WITH_BRIDGE
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Bridge %s sending CONNECT", mosq->id);
# endif
#else
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s sending CONNECT", mosq->id);
#endif
	return _mosquitto_packet_queue(mosq, packet);
}

int _mosquitto_send_disconnect(struct mosquitto *mosq)
{
	assert(mosq);
#ifdef WITH_BROKER
# ifdef WITH_BRIDGE
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Bridge %s sending DISCONNECT", mosq->id);
# endif
#else
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s sending DISCONNECT", mosq->id);
#endif
	return _mosquitto_send_simple_command(mosq, DISCONNECT);
}

int _mosquitto_send_subscribe(struct mosquitto *mosq, int *mid, bool dup, const char *topic, uint8_t topic_qos)
{
	/* FIXME - only deals with a single topic */
	struct _mosquitto_packet *packet = NULL;
	uint32_t packetlen;
	uint16_t local_mid;
	int rc;

	assert(mosq);
	assert(topic);

	packet = _mosquitto_calloc(1, sizeof(struct _mosquitto_packet));
	if(!packet) return MOSQ_ERR_NOMEM;

	packetlen = 2 + 2+strlen(topic) + 1;

	packet->command = SUBSCRIBE | (dup<<3) | (1<<1);
	packet->remaining_length = packetlen;
	rc = _mosquitto_packet_alloc(packet);
	if(rc){
		_mosquitto_free(packet);
		return rc;
	}

	/* Variable header */
	local_mid = _mosquitto_mid_generate(mosq);
	if(mid) *mid = (int)local_mid;
	_mosquitto_write_uint16(packet, local_mid);

	/* Payload */
	_mosquitto_write_string(packet, topic, strlen(topic));
	_mosquitto_write_byte(packet, topic_qos);

#ifdef WITH_BROKER
# ifdef WITH_BRIDGE
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Bridge %s sending SUBSCRIBE (Mid: %d, Topic: %s, QoS: %d)", mosq->id, local_mid, topic, topic_qos);
# endif
#else
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s sending SUBSCRIBE (Mid: %d, Topic: %s, QoS: %d)", mosq->id, local_mid, topic, topic_qos);
#endif

	return _mosquitto_packet_queue(mosq, packet);
}


int _mosquitto_send_unsubscribe(struct mosquitto *mosq, int *mid, bool dup, const char *topic)
{
	/* FIXME - only deals with a single topic */
	struct _mosquitto_packet *packet = NULL;
	uint32_t packetlen;
	uint16_t local_mid;
	int rc;

	assert(mosq);
	assert(topic);

	packet = _mosquitto_calloc(1, sizeof(struct _mosquitto_packet));
	if(!packet) return MOSQ_ERR_NOMEM;

	packetlen = 2 + 2+strlen(topic);

	packet->command = UNSUBSCRIBE | (dup<<3) | (1<<1);
	packet->remaining_length = packetlen;
	rc = _mosquitto_packet_alloc(packet);
	if(rc){
		_mosquitto_free(packet);
		return rc;
	}

	/* Variable header */
	local_mid = _mosquitto_mid_generate(mosq);
	if(mid) *mid = (int)local_mid;
	_mosquitto_write_uint16(packet, local_mid);

	/* Payload */
	_mosquitto_write_string(packet, topic, strlen(topic));

#ifdef WITH_BROKER
# ifdef WITH_BRIDGE
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Bridge %s sending UNSUBSCRIBE (Mid: %d, Topic: %s)", mosq->id, local_mid, topic);
# endif
#else
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s sending UNSUBSCRIBE (Mid: %d, Topic: %s)", mosq->id, local_mid, topic);
#endif
	return _mosquitto_packet_queue(mosq, packet);
}

