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
#include "util_mosq.h"
#ifdef WITH_BROKER
#include "mosquitto_broker.h"
#endif

int _mosquitto_handle_pingreq(struct mosquitto *mosq)
{
	assert(mosq);
#ifdef WITH_STRICT_PROTOCOL
	if(mosq->in_packet.remaining_length != 0){
		return MOSQ_ERR_PROTOCOL;
	}
#endif
#ifdef WITH_BROKER
	_mosquitto_log_printf(NULL, MOSQ_LOG_DEBUG, "Received PINGREQ from %s", mosq->id);
#else
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s received PINGREQ", mosq->id);
#endif
	return _mosquitto_send_pingresp(mosq);
}

int _mosquitto_handle_pingresp(struct mosquitto *mosq)
{
	assert(mosq);
#ifdef WITH_STRICT_PROTOCOL
	if(mosq->in_packet.remaining_length != 0){
		return MOSQ_ERR_PROTOCOL;
	}
#endif
	mosq->ping_t = 0; /* No longer waiting for a PINGRESP. */
#ifdef WITH_BROKER
	_mosquitto_log_printf(NULL, MOSQ_LOG_DEBUG, "Received PINGRESP from %s", mosq->id);
#else
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s received PINGRESP", mosq->id);
#endif
	return MOSQ_ERR_SUCCESS;
}

int _mosquitto_handle_pubackcomp(struct mosquitto *mosq, const char *type)
{
	uint16_t mid;
	int rc;

	assert(mosq);
#ifdef WITH_STRICT_PROTOCOL
	if(mosq->in_packet.remaining_length != 2){
		return MOSQ_ERR_PROTOCOL;
	}
#endif
	rc = _mosquitto_read_uint16(&mosq->in_packet, &mid);
	if(rc) return rc;
#ifdef WITH_BROKER
	_mosquitto_log_printf(NULL, MOSQ_LOG_DEBUG, "Received %s from %s (Mid: %d)", type, mosq->id, mid);

	if(mid){
		rc = mqtt3_db_message_delete(mosq, mid, mosq_md_out);
		if(rc) return rc;
	}
#else
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s received %s (Mid: %d)", mosq->id, type, mid);

	if(!_mosquitto_message_delete(mosq, mid, mosq_md_out)){
		/* Only inform the client the message has been sent once. */
		pthread_mutex_lock(&mosq->callback_mutex);
		if(mosq->on_publish){
			mosq->in_callback = true;
			mosq->on_publish(mosq, mosq->userdata, mid);
			mosq->in_callback = false;
		}
		pthread_mutex_unlock(&mosq->callback_mutex);
	}
#endif

	return MOSQ_ERR_SUCCESS;
}

int _mosquitto_handle_pubrec(struct mosquitto *mosq)
{
	uint16_t mid;
	int rc;

	assert(mosq);
#ifdef WITH_STRICT_PROTOCOL
	if(mosq->in_packet.remaining_length != 2){
		return MOSQ_ERR_PROTOCOL;
	}
#endif
	rc = _mosquitto_read_uint16(&mosq->in_packet, &mid);
	if(rc) return rc;
#ifdef WITH_BROKER
	_mosquitto_log_printf(NULL, MOSQ_LOG_DEBUG, "Received PUBREC from %s (Mid: %d)", mosq->id, mid);

	rc = mqtt3_db_message_update(mosq, mid, mosq_md_out, mosq_ms_wait_for_pubcomp);
#else
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s received PUBREC (Mid: %d)", mosq->id, mid);

	rc = _mosquitto_message_update(mosq, mid, mosq_md_out, mosq_ms_wait_for_pubcomp);
#endif
	if(rc) return rc;
	rc = _mosquitto_send_pubrel(mosq, mid, false);
	if(rc) return rc;

	return MOSQ_ERR_SUCCESS;
}

int _mosquitto_handle_pubrel(struct mosquitto_db *db, struct mosquitto *mosq)
{
	uint16_t mid;
#ifndef WITH_BROKER
	struct mosquitto_message_all *message = NULL;
#endif
	int rc;

	assert(mosq);
#ifdef WITH_STRICT_PROTOCOL
	if(mosq->in_packet.remaining_length != 2){
		return MOSQ_ERR_PROTOCOL;
	}
#endif
	rc = _mosquitto_read_uint16(&mosq->in_packet, &mid);
	if(rc) return rc;
#ifdef WITH_BROKER
	_mosquitto_log_printf(NULL, MOSQ_LOG_DEBUG, "Received PUBREL from %s (Mid: %d)", mosq->id, mid);

	if(mqtt3_db_message_release(db, mosq, mid, mosq_md_in)){
		/* Message not found. Still send a PUBCOMP anyway because this could be
		 * due to a repeated PUBREL after a client has reconnected. */
	}
#else
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s received PUBREL (Mid: %d)", mosq->id, mid);

	if(!_mosquitto_message_remove(mosq, mid, mosq_md_in, &message)){
		/* Only pass the message on if we have removed it from the queue - this
		 * prevents multiple callbacks for the same message. */
		pthread_mutex_lock(&mosq->callback_mutex);
		if(mosq->on_message){
			mosq->in_callback = true;
			mosq->on_message(mosq, mosq->userdata, &message->msg);
			mosq->in_callback = false;
		}
		pthread_mutex_unlock(&mosq->callback_mutex);
		_mosquitto_message_cleanup(&message);
	}
#endif
	rc = _mosquitto_send_pubcomp(mosq, mid);
	if(rc) return rc;

	return MOSQ_ERR_SUCCESS;
}

int _mosquitto_handle_suback(struct mosquitto *mosq)
{
	uint16_t mid;
	uint8_t qos;
	int *granted_qos;
	int qos_count;
	int i = 0;
	int rc;

	assert(mosq);
#ifdef WITH_BROKER
	_mosquitto_log_printf(NULL, MOSQ_LOG_DEBUG, "Received SUBACK from %s", mosq->id);
#else
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s received SUBACK", mosq->id);
#endif
	rc = _mosquitto_read_uint16(&mosq->in_packet, &mid);
	if(rc) return rc;

	qos_count = mosq->in_packet.remaining_length - mosq->in_packet.pos;
	granted_qos = _mosquitto_malloc(qos_count*sizeof(int));
	if(!granted_qos) return MOSQ_ERR_NOMEM;
	while(mosq->in_packet.pos < mosq->in_packet.remaining_length){
		rc = _mosquitto_read_byte(&mosq->in_packet, &qos);
		if(rc){
			_mosquitto_free(granted_qos);
			return rc;
		}
		granted_qos[i] = (int)qos;
		i++;
	}
#ifndef WITH_BROKER
	pthread_mutex_lock(&mosq->callback_mutex);
	if(mosq->on_subscribe){
		mosq->in_callback = true;
		mosq->on_subscribe(mosq, mosq->userdata, mid, qos_count, granted_qos);
		mosq->in_callback = false;
	}
	pthread_mutex_unlock(&mosq->callback_mutex);
#endif
	_mosquitto_free(granted_qos);

	return MOSQ_ERR_SUCCESS;
}

int _mosquitto_handle_unsuback(struct mosquitto *mosq)
{
	uint16_t mid;
	int rc;

	assert(mosq);
#ifdef WITH_STRICT_PROTOCOL
	if(mosq->in_packet.remaining_length != 2){
		return MOSQ_ERR_PROTOCOL;
	}
#endif
#ifdef WITH_BROKER
	_mosquitto_log_printf(NULL, MOSQ_LOG_DEBUG, "Received UNSUBACK from %s", mosq->id);
#else
	_mosquitto_log_printf(mosq, MOSQ_LOG_DEBUG, "Client %s received UNSUBACK", mosq->id);
#endif
	rc = _mosquitto_read_uint16(&mosq->in_packet, &mid);
	if(rc) return rc;
#ifndef WITH_BROKER
	pthread_mutex_lock(&mosq->callback_mutex);
	if(mosq->on_unsubscribe){
		mosq->in_callback = true;
	   	mosq->on_unsubscribe(mosq, mosq->userdata, mid);
		mosq->in_callback = false;
	}
	pthread_mutex_unlock(&mosq->callback_mutex);
#endif

	return MOSQ_ERR_SUCCESS;
}

