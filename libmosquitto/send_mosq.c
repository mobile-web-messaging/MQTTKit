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
#include <stdio.h>
#include <string.h>

#include "mosquitto.h"
#include "mosquitto_internal.h"
#include "logging_mosq.h"
#include "mqtt3_protocol.h"
#include "memory_mosq.h"
#include "net_mosq.h"
#include "send_mosq.h"
#include "time_mosq.h"
#include "util_mosq.h"

#ifdef WITH_BROKER
#include "mosquitto_broker.h"
#  ifdef WITH_SYS_TREE
extern uint64_t g_pub_bytes_sent;
#  endif
#endif

int _mosquitto_send_pingreq(struct mosquitto *mosq)
{
	int rc;
	assert(mosq);
#ifdef WITH_BROKER
	_mosquitto_log_printf(NULL, MOSQ_LOG_DEBUG, "Sending PINGREQ to %s", mosq->id);
#else
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s sending PINGREQ", mosq->id);
#endif
	rc = _mosquitto_send_simple_command(mosq, PINGREQ);
	if(rc == MOSQ_ERR_SUCCESS){
		mosq->ping_t = mosquitto_time();
	}
	return rc;
}

int _mosquitto_send_pingresp(struct mosquitto *mosq)
{
#ifdef WITH_BROKER
	if(mosq) _mosquitto_log_printf(NULL, MOSQ_LOG_DEBUG, "Sending PINGRESP to %s", mosq->id);
#else
	if(mosq) _mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s sending PINGRESP", mosq->id);
#endif
	return _mosquitto_send_simple_command(mosq, PINGRESP);
}

int _mosquitto_send_puback(struct mosquitto *mosq, uint16_t mid)
{
#ifdef WITH_BROKER
	if(mosq) _mosquitto_log_printf(NULL, MOSQ_LOG_DEBUG, "Sending PUBACK to %s (Mid: %d)", mosq->id, mid);
#else
	if(mosq) _mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s sending PUBACK (Mid: %d)", mosq->id, mid);
#endif
	return _mosquitto_send_command_with_mid(mosq, PUBACK, mid, false);
}

int _mosquitto_send_pubcomp(struct mosquitto *mosq, uint16_t mid)
{
#ifdef WITH_BROKER
	if(mosq) _mosquitto_log_printf(NULL, MOSQ_LOG_DEBUG, "Sending PUBCOMP to %s (Mid: %d)", mosq->id, mid);
#else
	if(mosq) _mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s sending PUBCOMP (Mid: %d)", mosq->id, mid);
#endif
	return _mosquitto_send_command_with_mid(mosq, PUBCOMP, mid, false);
}

int _mosquitto_send_publish(struct mosquitto *mosq, uint16_t mid, const char *topic, uint32_t payloadlen, const void *payload, int qos, bool retain, bool dup)
{
#ifdef WITH_BROKER
	size_t len;
#ifdef WITH_BRIDGE
	int i;
	struct _mqtt3_bridge_topic *cur_topic;
	bool match;
	int rc;
	char *mapped_topic = NULL;
	char *topic_temp = NULL;
#endif
#endif
	assert(mosq);
	assert(topic);

	if(mosq->sock == INVALID_SOCKET) return MOSQ_ERR_NO_CONN;
#ifdef WITH_BROKER
	if(mosq->listener && mosq->listener->mount_point){
		len = strlen(mosq->listener->mount_point);
		if(len < strlen(topic)){
			topic += len;
		}else{
			/* Invalid topic string. Should never happen, but silently swallow the message anyway. */
			return MOSQ_ERR_SUCCESS;
		}
	}
#ifdef WITH_BRIDGE
	if(mosq->bridge && mosq->bridge->topics && mosq->bridge->topic_remapping){
		for(i=0; i<mosq->bridge->topic_count; i++){
			cur_topic = &mosq->bridge->topics[i];
			if(cur_topic->remote_prefix || cur_topic->local_prefix){
				/* Topic mapping required on this topic if the message matches */

				rc = mosquitto_topic_matches_sub(cur_topic->local_topic, topic, &match);
				if(rc){
					return rc;
				}
				if(match){
					mapped_topic = _mosquitto_strdup(topic);
					if(!mapped_topic) return MOSQ_ERR_NOMEM;
					if(cur_topic->local_prefix){
						/* This prefix needs removing. */
						if(!strncmp(cur_topic->local_prefix, mapped_topic, strlen(cur_topic->local_prefix))){
							topic_temp = _mosquitto_strdup(mapped_topic+strlen(cur_topic->local_prefix));
							_mosquitto_free(mapped_topic);
							if(!topic_temp){
								return MOSQ_ERR_NOMEM;
							}
							mapped_topic = topic_temp;
						}
					}

					if(cur_topic->remote_prefix){
						/* This prefix needs adding. */
						len = strlen(mapped_topic) + strlen(cur_topic->remote_prefix)+1;
						topic_temp = _mosquitto_calloc(len+1, sizeof(char));
						if(!topic_temp){
							_mosquitto_free(mapped_topic);
							return MOSQ_ERR_NOMEM;
						}
						snprintf(topic_temp, len, "%s%s", cur_topic->remote_prefix, mapped_topic);
						_mosquitto_free(mapped_topic);
						mapped_topic = topic_temp;
					}
					_mosquitto_log_printf(NULL, MOSQ_LOG_DEBUG, "Sending PUBLISH to %s (d%d, q%d, r%d, m%d, '%s', ... (%ld bytes))", mosq->id, dup, qos, retain, mid, mapped_topic, (long)payloadlen);
#ifdef WITH_SYS_TREE
					g_pub_bytes_sent += payloadlen;
#endif
					rc =  _mosquitto_send_real_publish(mosq, mid, mapped_topic, payloadlen, payload, qos, retain, dup);
					_mosquitto_free(mapped_topic);
					return rc;
				}
			}
		}
	}
#endif
	_mosquitto_log_printf(NULL, MOSQ_LOG_DEBUG, "Sending PUBLISH to %s (d%d, q%d, r%d, m%d, '%s', ... (%ld bytes))", mosq->id, dup, qos, retain, mid, topic, (long)payloadlen);
#  ifdef WITH_SYS_TREE
	g_pub_bytes_sent += payloadlen;
#  endif
#else
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s sending PUBLISH (d%d, q%d, r%d, m%d, '%s', ... (%ld bytes))", mosq->id, dup, qos, retain, mid, topic, (long)payloadlen);
#endif

	return _mosquitto_send_real_publish(mosq, mid, topic, payloadlen, payload, qos, retain, dup);
}

int _mosquitto_send_pubrec(struct mosquitto *mosq, uint16_t mid)
{
#ifdef WITH_BROKER
	if(mosq) _mosquitto_log_printf(NULL, MOSQ_LOG_DEBUG, "Sending PUBREC to %s (Mid: %d)", mosq->id, mid);
#else
	if(mosq) _mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s sending PUBREC (Mid: %d)", mosq->id, mid);
#endif
	return _mosquitto_send_command_with_mid(mosq, PUBREC, mid, false);
}

int _mosquitto_send_pubrel(struct mosquitto *mosq, uint16_t mid, bool dup)
{
#ifdef WITH_BROKER
	if(mosq) _mosquitto_log_printf(NULL, MOSQ_LOG_DEBUG, "Sending PUBREL to %s (Mid: %d)", mosq->id, mid);
#else
	if(mosq) _mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s sending PUBREL (Mid: %d)", mosq->id, mid);
#endif
	return _mosquitto_send_command_with_mid(mosq, PUBREL|2, mid, dup);
}

/* For PUBACK, PUBCOMP, PUBREC, and PUBREL */
int _mosquitto_send_command_with_mid(struct mosquitto *mosq, uint8_t command, uint16_t mid, bool dup)
{
	struct _mosquitto_packet *packet = NULL;
	int rc;

	assert(mosq);
	packet = _mosquitto_calloc(1, sizeof(struct _mosquitto_packet));
	if(!packet) return MOSQ_ERR_NOMEM;

	packet->command = command;
	if(dup){
		packet->command |= 8;
	}
	packet->remaining_length = 2;
	rc = _mosquitto_packet_alloc(packet);
	if(rc){
		_mosquitto_free(packet);
		return rc;
	}

	packet->payload[packet->pos+0] = MOSQ_MSB(mid);
	packet->payload[packet->pos+1] = MOSQ_LSB(mid);

	return _mosquitto_packet_queue(mosq, packet);
}

/* For DISCONNECT, PINGREQ and PINGRESP */
int _mosquitto_send_simple_command(struct mosquitto *mosq, uint8_t command)
{
	struct _mosquitto_packet *packet = NULL;
	int rc;

	assert(mosq);
	packet = _mosquitto_calloc(1, sizeof(struct _mosquitto_packet));
	if(!packet) return MOSQ_ERR_NOMEM;

	packet->command = command;
	packet->remaining_length = 0;

	rc = _mosquitto_packet_alloc(packet);
	if(rc){
		_mosquitto_free(packet);
		return rc;
	}

	return _mosquitto_packet_queue(mosq, packet);
}

int _mosquitto_send_real_publish(struct mosquitto *mosq, uint16_t mid, const char *topic, uint32_t payloadlen, const void *payload, int qos, bool retain, bool dup)
{
	struct _mosquitto_packet *packet = NULL;
	int packetlen;
	int rc;

	assert(mosq);
	assert(topic);

	packetlen = 2+strlen(topic) + payloadlen;
	if(qos > 0) packetlen += 2; /* For message id */
	packet = _mosquitto_calloc(1, sizeof(struct _mosquitto_packet));
	if(!packet) return MOSQ_ERR_NOMEM;

	packet->mid = mid;
	packet->command = PUBLISH | ((dup&0x1)<<3) | (qos<<1) | retain;
	packet->remaining_length = packetlen;
	rc = _mosquitto_packet_alloc(packet);
	if(rc){
		_mosquitto_free(packet);
		return rc;
	}
	/* Variable header (topic string) */
	_mosquitto_write_string(packet, topic, strlen(topic));
	if(qos > 0){
		_mosquitto_write_uint16(packet, mid);
	}

	/* Payload */
	if(payloadlen){
		_mosquitto_write_bytes(packet, payload, payloadlen);
	}

	return _mosquitto_packet_queue(mosq, packet);
}
