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
#include "logging_mosq.h"
#include "memory_mosq.h"
#include "messages_mosq.h"
#include "mqtt3_protocol.h"
#include "net_mosq.h"
#include "read_handle.h"
#include "send_mosq.h"
#include "time_mosq.h"
#include "util_mosq.h"

int _mosquitto_packet_handle(struct mosquitto *mosq)
{
	assert(mosq);

	switch((mosq->in_packet.command)&0xF0){
		case PINGREQ:
			return _mosquitto_handle_pingreq(mosq);
		case PINGRESP:
			return _mosquitto_handle_pingresp(mosq);
		case PUBACK:
			return _mosquitto_handle_pubackcomp(mosq, "PUBACK");
		case PUBCOMP:
			return _mosquitto_handle_pubackcomp(mosq, "PUBCOMP");
		case PUBLISH:
			return _mosquitto_handle_publish(mosq);
		case PUBREC:
			return _mosquitto_handle_pubrec(mosq);
		case PUBREL:
			return _mosquitto_handle_pubrel(NULL, mosq);
		case CONNACK:
			return _mosquitto_handle_connack(mosq);
		case SUBACK:
			return _mosquitto_handle_suback(mosq);
		case UNSUBACK:
			return _mosquitto_handle_unsuback(mosq);
		default:
			/* If we don't recognise the command, return an error straight away. */
			_mosquitto_log_printf(mosq, MOSQ_LOG_ERR, "Error: Unrecognised command %d\n", (mosq->in_packet.command)&0xF0);
			return MOSQ_ERR_PROTOCOL;
	}
}

int _mosquitto_handle_publish(struct mosquitto *mosq)
{
	uint8_t header;
	struct mosquitto_message_all *message;
	int rc = 0;
	uint16_t mid;

	assert(mosq);

	message = _mosquitto_calloc(1, sizeof(struct mosquitto_message_all));
	if(!message) return MOSQ_ERR_NOMEM;

	header = mosq->in_packet.command;

	message->direction = mosq_md_in;
	message->dup = (header & 0x08)>>3;
	message->msg.qos = (header & 0x06)>>1;
	message->msg.retain = (header & 0x01);

	rc = _mosquitto_read_string(&mosq->in_packet, &message->msg.topic);
	if(rc){
		_mosquitto_message_cleanup(&message);
		return rc;
	}
	rc = _mosquitto_fix_sub_topic(&message->msg.topic);
	if(rc){
		_mosquitto_message_cleanup(&message);
		return rc;
	}
	if(!strlen(message->msg.topic)){
		_mosquitto_message_cleanup(&message);
		return MOSQ_ERR_PROTOCOL;
	}

	if(message->msg.qos > 0){
		rc = _mosquitto_read_uint16(&mosq->in_packet, &mid);
		if(rc){
			_mosquitto_message_cleanup(&message);
			return rc;
		}
		message->msg.mid = (int)mid;
	}

	message->msg.payloadlen = mosq->in_packet.remaining_length - mosq->in_packet.pos;
	if(message->msg.payloadlen){
		message->msg.payload = _mosquitto_calloc(message->msg.payloadlen+1, sizeof(uint8_t));
		if(!message->msg.payload){
			_mosquitto_message_cleanup(&message);
			return MOSQ_ERR_NOMEM;
		}
		rc = _mosquitto_read_bytes(&mosq->in_packet, message->msg.payload, message->msg.payloadlen);
		if(rc){
			_mosquitto_message_cleanup(&message);
			return rc;
		}
	}
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG,
			"Client %s received PUBLISH (d%d, q%d, r%d, m%d, '%s', ... (%ld bytes))",
			mosq->id, message->dup, message->msg.qos, message->msg.retain,
			message->msg.mid, message->msg.topic,
			(long)message->msg.payloadlen);

	message->timestamp = mosquitto_time();
	switch(message->msg.qos){
		case 0:
			pthread_mutex_lock(&mosq->callback_mutex);
			if(mosq->on_message){
				mosq->in_callback = true;
				mosq->on_message(mosq, mosq->userdata, &message->msg);
				mosq->in_callback = false;
			}
			pthread_mutex_unlock(&mosq->callback_mutex);
			_mosquitto_message_cleanup(&message);
			return MOSQ_ERR_SUCCESS;
		case 1:
			rc = _mosquitto_send_puback(mosq, message->msg.mid);
			pthread_mutex_lock(&mosq->callback_mutex);
			if(mosq->on_message){
				mosq->in_callback = true;
				mosq->on_message(mosq, mosq->userdata, &message->msg);
				mosq->in_callback = false;
			}
			pthread_mutex_unlock(&mosq->callback_mutex);
			_mosquitto_message_cleanup(&message);
			return rc;
		case 2:
			rc = _mosquitto_send_pubrec(mosq, message->msg.mid);
			pthread_mutex_lock(&mosq->message_mutex);
			message->state = mosq_ms_wait_for_pubrel;
			_mosquitto_message_queue(mosq, message, true);
			pthread_mutex_unlock(&mosq->message_mutex);
			return rc;
		default:
			_mosquitto_message_cleanup(&message);
			return MOSQ_ERR_PROTOCOL;
	}
}

