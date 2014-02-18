/*
Copyright (c) 2010-2013 Roger Light <roger@atchoo.org>
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
#include <stdlib.h>
#include <string.h>

#include "mosquitto_internal.h"
#include "mosquitto.h"
#include "memory_mosq.h"
#include "messages_mosq.h"
#include "send_mosq.h"
#include "time_mosq.h"

void _mosquitto_message_cleanup(struct mosquitto_message_all **message)
{
	struct mosquitto_message_all *msg;

	if(!message || !*message) return;

	msg = *message;

	if(msg->msg.topic) _mosquitto_free(msg->msg.topic);
	if(msg->msg.payload) _mosquitto_free(msg->msg.payload);
	_mosquitto_free(msg);
}

void _mosquitto_message_cleanup_all(struct mosquitto *mosq)
{
	struct mosquitto_message_all *tmp;

	assert(mosq);

	while(mosq->messages){
		tmp = mosq->messages->next;
		_mosquitto_message_cleanup(&mosq->messages);
		mosq->messages = tmp;
	}
}

int mosquitto_message_copy(struct mosquitto_message *dst, const struct mosquitto_message *src)
{
	if(!dst || !src) return MOSQ_ERR_INVAL;

	dst->mid = src->mid;
	dst->topic = _mosquitto_strdup(src->topic);
	if(!dst->topic) return MOSQ_ERR_NOMEM;
	dst->qos = src->qos;
	dst->retain = src->retain;
	if(src->payloadlen){
		dst->payload = _mosquitto_malloc(src->payloadlen);
		if(!dst->payload){
			_mosquitto_free(dst->topic);
			return MOSQ_ERR_NOMEM;
		}
		memcpy(dst->payload, src->payload, src->payloadlen);
		dst->payloadlen = src->payloadlen;
	}else{
		dst->payloadlen = 0;
		dst->payload = NULL;
	}
	return MOSQ_ERR_SUCCESS;
}

int _mosquitto_message_delete(struct mosquitto *mosq, uint16_t mid, enum mosquitto_msg_direction dir)
{
	struct mosquitto_message_all *message;
	int rc;
	assert(mosq);

	rc = _mosquitto_message_remove(mosq, mid, dir, &message);
	if(rc == MOSQ_ERR_SUCCESS){
		_mosquitto_message_cleanup(&message);
	}
	return rc;
}

void mosquitto_message_free(struct mosquitto_message **message)
{
	struct mosquitto_message *msg;

	if(!message || !*message) return;

	msg = *message;

	if(msg->topic) _mosquitto_free(msg->topic);
	if(msg->payload) _mosquitto_free(msg->payload);
	_mosquitto_free(msg);
}

void _mosquitto_message_queue(struct mosquitto *mosq, struct mosquitto_message_all *message, bool doinc)
{
	/* mosq->message_mutex should be locked before entering this function */
	assert(mosq);
	assert(message);

	mosq->queue_len++;
	if(doinc == true && message->msg.qos > 0 && (mosq->max_inflight_messages == 0 || mosq->inflight_messages < mosq->max_inflight_messages)){
		mosq->inflight_messages++;
	}
	message->next = NULL;
	if(mosq->messages_last){
		mosq->messages_last->next = message;
	}else{
		mosq->messages = message;
	}
	mosq->messages_last = message;
}

void _mosquitto_messages_reconnect_reset(struct mosquitto *mosq)
{
	struct mosquitto_message_all *message;
	struct mosquitto_message_all *prev = NULL;
	assert(mosq);

	pthread_mutex_lock(&mosq->message_mutex);
	mosq->queue_len = 0;
	mosq->inflight_messages = 0;
	message = mosq->messages;
	while(message){
		message->timestamp = 0;
		if(message->direction == mosq_md_out){
			mosq->queue_len++;
			if(message->msg.qos > 0){
				mosq->inflight_messages++;
			}
			if(mosq->max_inflight_messages == 0 || mosq->inflight_messages < mosq->max_inflight_messages){
				if(message->msg.qos == 1){
					message->state = mosq_ms_wait_for_puback;
				}else if(message->msg.qos == 2){
					/* Should be able to preserve state. */
				}
			}else{
				message->state = mosq_ms_invalid;
			}
		}else{
			if(message->msg.qos != 2){
				if(prev){
					prev->next = message->next;
					_mosquitto_message_cleanup(&message);
					message = prev;
				}else{
					mosq->messages = message->next;
					_mosquitto_message_cleanup(&message);
					message = mosq->messages;
				}
			}else{
				/* Message state can be preserved here because it should match
				 * whatever the client has got. */
			}
		}
		prev = message;
		message = message->next;
	}
	mosq->messages_last = prev;
	pthread_mutex_unlock(&mosq->message_mutex);
}

int _mosquitto_message_remove(struct mosquitto *mosq, uint16_t mid, enum mosquitto_msg_direction dir, struct mosquitto_message_all **message)
{
	struct mosquitto_message_all *cur, *prev = NULL;
	bool found = false;
	int rc;
	assert(mosq);
	assert(message);

	pthread_mutex_lock(&mosq->message_mutex);
	cur = mosq->messages;
	while(cur){
		if(cur->msg.mid == mid && cur->direction == dir){
			if(prev){
				prev->next = cur->next;
			}else{
				mosq->messages = cur->next;
			}
			*message = cur;
			mosq->queue_len--;
			if(cur->next == NULL){
				mosq->messages_last = prev;
			}else if(!mosq->messages){
				mosq->messages_last = NULL;
			}
			if((cur->msg.qos == 2 && dir == mosq_md_in) || (cur->msg.qos > 0 && dir == mosq_md_out)){
				mosq->inflight_messages--;
			}
			found = true;
			break;
		}
		prev = cur;
		cur = cur->next;
	}

	if(found){
		cur = mosq->messages;
		while(cur){
			if(mosq->max_inflight_messages == 0 || mosq->inflight_messages < mosq->max_inflight_messages){
				if(cur->msg.qos > 0 && cur->state == mosq_ms_invalid && cur->direction == mosq_md_out){
					mosq->inflight_messages++;
					if(cur->msg.qos == 1){
						cur->state = mosq_ms_wait_for_puback;
					}else if(cur->msg.qos == 2){
						cur->state = mosq_ms_wait_for_pubrec;
					}
					rc = _mosquitto_send_publish(mosq, cur->msg.mid, cur->msg.topic, cur->msg.payloadlen, cur->msg.payload, cur->msg.qos, cur->msg.retain, cur->dup);
					if(rc){
						pthread_mutex_unlock(&mosq->message_mutex);
						return rc;
					}
				}
			}else{
				pthread_mutex_unlock(&mosq->message_mutex);
				return MOSQ_ERR_SUCCESS;
			}
			cur = cur->next;
		}
		pthread_mutex_unlock(&mosq->message_mutex);
		return MOSQ_ERR_SUCCESS;
	}else{
		pthread_mutex_unlock(&mosq->message_mutex);
		return MOSQ_ERR_NOT_FOUND;
	}
}

void _mosquitto_message_retry_check(struct mosquitto *mosq)
{
	struct mosquitto_message_all *message;
	time_t now = mosquitto_time();
	assert(mosq);

	pthread_mutex_lock(&mosq->message_mutex);
	message = mosq->messages;
	while(message){
		if(message->timestamp + mosq->message_retry < now){
			switch(message->state){
				case mosq_ms_wait_for_puback:
				case mosq_ms_wait_for_pubrec:
					message->timestamp = now;
					message->dup = true;
					_mosquitto_send_publish(mosq, message->msg.mid, message->msg.topic, message->msg.payloadlen, message->msg.payload, message->msg.qos, message->msg.retain, message->dup);
					break;
				case mosq_ms_wait_for_pubrel:
					message->timestamp = now;
					message->dup = true;
					_mosquitto_send_pubrec(mosq, message->msg.mid);
					break;
				case mosq_ms_wait_for_pubcomp:
					message->timestamp = now;
					message->dup = true;
					_mosquitto_send_pubrel(mosq, message->msg.mid, true);
					break;
				default:
					break;
			}
		}
		message = message->next;
	}
	pthread_mutex_unlock(&mosq->message_mutex);
}

void mosquitto_message_retry_set(struct mosquitto *mosq, unsigned int message_retry)
{
	assert(mosq);
	if(mosq) mosq->message_retry = message_retry;
}

int _mosquitto_message_update(struct mosquitto *mosq, uint16_t mid, enum mosquitto_msg_direction dir, enum mosquitto_msg_state state)
{
	struct mosquitto_message_all *message;
	assert(mosq);

	pthread_mutex_lock(&mosq->message_mutex);
	message = mosq->messages;
	while(message){
		if(message->msg.mid == mid && message->direction == dir){
			message->state = state;
			message->timestamp = mosquitto_time();
			pthread_mutex_unlock(&mosq->message_mutex);
			return MOSQ_ERR_SUCCESS;
		}
		message = message->next;
	}
	pthread_mutex_unlock(&mosq->message_mutex);
	return MOSQ_ERR_NOT_FOUND;
}

int mosquitto_max_inflight_messages_set(struct mosquitto *mosq, unsigned int max_inflight_messages)
{
	if(!mosq) return MOSQ_ERR_INVAL;

	mosq->max_inflight_messages = max_inflight_messages;

	return MOSQ_ERR_SUCCESS;
}

