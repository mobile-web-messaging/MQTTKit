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
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifndef WIN32
#include <sys/select.h>
#include <unistd.h>
#else
#include <winsock2.h>
typedef int ssize_t;
#endif

#include "mosquitto.h"
#include "mosquitto_internal.h"
#include "logging_mosq.h"
#include "messages_mosq.h"
#include "memory_mosq.h"
#include "mqtt3_protocol.h"
#include "net_mosq.h"
#include "read_handle.h"
#include "send_mosq.h"
#include "util_mosq.h"

int _mosquitto_will_set(struct mosquitto *mosq, const char *topic, int payloadlen, const void *payload, int qos, bool retain)
{
	int rc = MOSQ_ERR_SUCCESS;

	if(!mosq || !topic) return MOSQ_ERR_INVAL;
	if(payloadlen < 0 || payloadlen > MQTT_MAX_PAYLOAD) return MOSQ_ERR_PAYLOAD_SIZE;
	if(payloadlen > 0 && !payload) return MOSQ_ERR_INVAL;

	if(mosq->will){
		if(mosq->will->topic){
			_mosquitto_free(mosq->will->topic);
			mosq->will->topic = NULL;
		}
		if(mosq->will->payload){
			_mosquitto_free(mosq->will->payload);
			mosq->will->payload = NULL;
		}
		_mosquitto_free(mosq->will);
		mosq->will = NULL;
	}

	mosq->will = _mosquitto_calloc(1, sizeof(struct mosquitto_message));
	if(!mosq->will) return MOSQ_ERR_NOMEM;
	mosq->will->topic = _mosquitto_strdup(topic);
	if(!mosq->will->topic){
		rc = MOSQ_ERR_NOMEM;
		goto cleanup;
	}
	mosq->will->payloadlen = payloadlen;
	if(mosq->will->payloadlen > 0){
		if(!payload){
			rc = MOSQ_ERR_INVAL;
			goto cleanup;
		}
		mosq->will->payload = _mosquitto_malloc(sizeof(char)*mosq->will->payloadlen);
		if(!mosq->will->payload){
			rc = MOSQ_ERR_NOMEM;
			goto cleanup;
		}

		memcpy(mosq->will->payload, payload, payloadlen);
	}
	mosq->will->qos = qos;
	mosq->will->retain = retain;

	return MOSQ_ERR_SUCCESS;

cleanup:
	if(mosq->will){
		if(mosq->will->topic) _mosquitto_free(mosq->will->topic);
		if(mosq->will->payload) _mosquitto_free(mosq->will->payload);
	}
	_mosquitto_free(mosq->will);
	mosq->will = NULL;

	return rc;
}

int _mosquitto_will_clear(struct mosquitto *mosq)
{
	if(!mosq->will) return MOSQ_ERR_SUCCESS;

	if(mosq->will->topic){
		_mosquitto_free(mosq->will->topic);
		mosq->will->topic = NULL;
	}
	if(mosq->will->payload){
		_mosquitto_free(mosq->will->payload);
		mosq->will->payload = NULL;
	}
	_mosquitto_free(mosq->will);
	mosq->will = NULL;

	return MOSQ_ERR_SUCCESS;
}

