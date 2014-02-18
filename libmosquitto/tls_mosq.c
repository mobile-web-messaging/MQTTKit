/*
Copyright (c) 2013 Roger Light <roger@atchoo.org>
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

#ifdef WITH_TLS

#ifdef WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <arpa/inet.h>
#endif

#include <string.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>
#include <openssl/ssl.h>

#ifdef WITH_BROKER
#  include "mosquitto_broker.h"
#endif
#include "mosquitto_internal.h"
#include "tls_mosq.h"

extern int tls_ex_index_mosq;

int _mosquitto_server_certificate_verify(int preverify_ok, X509_STORE_CTX *ctx)
{
	/* Preverify should have already checked expiry, revocation.
	 * We need to verify the hostname. */
	struct mosquitto *mosq;
	SSL *ssl;
	X509 *cert;

	/* Always reject if preverify_ok has failed. */
	if(!preverify_ok) return 0;

	ssl = X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
	mosq = SSL_get_ex_data(ssl, tls_ex_index_mosq);
	if(!mosq) return 0;

	if(mosq->tls_insecure == false){
		if(X509_STORE_CTX_get_error_depth(ctx) == 0){
			/* FIXME - use X509_check_host() etc. for sufficiently new openssl (>=1.1.x) */
			cert = X509_STORE_CTX_get_current_cert(ctx);
			/* This is the peer certificate, all others are upwards in the chain. */
#if defined(WITH_BROKER)
			return _mosquitto_verify_certificate_hostname(cert, mosq->bridge->addresses[mosq->bridge->cur_address].address);
#else
			return _mosquitto_verify_certificate_hostname(cert, mosq->host);
#endif
		}else{
			return preverify_ok;
		}
	}else{
		return preverify_ok;
	}
}

/* This code is based heavily on the example provided in "Secure Programming
 * Cookbook for C and C++".
 */
int _mosquitto_verify_certificate_hostname(X509 *cert, const char *hostname)
{
	int i;
	char name[256];
	X509_NAME *subj;
	bool have_san_dns = false;
	STACK_OF(GENERAL_NAME) *san;
	const GENERAL_NAME *nval;
	const unsigned char *data;
	unsigned char ipv6_addr[16];
	unsigned char ipv4_addr[4];
	int ipv6_ok;
	int ipv4_ok;

#ifdef WIN32
	ipv6_ok = InetPton(AF_INET6, hostname, &ipv6_addr);
	ipv4_ok = InetPton(AF_INET, hostname, &ipv4_addr);
#else
	ipv6_ok = inet_pton(AF_INET6, hostname, &ipv6_addr);
	ipv4_ok = inet_pton(AF_INET, hostname, &ipv4_addr);
#endif

	san = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
	if(san){
		for(i=0; i<sk_GENERAL_NAME_num(san); i++){
			nval = sk_GENERAL_NAME_value(san, i);
			if(nval->type == GEN_DNS){
				data = ASN1_STRING_data(nval->d.dNSName);
				if(data && !strcasecmp((char *)data, hostname)){
					return 1;
				}
				have_san_dns = true;
			}else if(nval->type == GEN_IPADD){
				data = ASN1_STRING_data(nval->d.iPAddress);
				if(nval->d.iPAddress->length == 4 && ipv4_ok){
					if(!memcmp(ipv4_addr, data, 4)){
						return 1;
					}
				}else if(nval->d.iPAddress->length == 16 && ipv6_ok){
					if(!memcmp(ipv6_addr, data, 16)){
						return 1;
					}
				}
			}
		}
		if(have_san_dns){
			/* Only check CN if subjectAltName DNS entry does not exist. */
			return 0;
		}
	}
	subj = X509_get_subject_name(cert);
	if(X509_NAME_get_text_by_NID(subj, NID_commonName, name, sizeof(name)) > 0){
		name[sizeof(name) - 1] = '\0';
		if (!strcasecmp(name, hostname)) return 1;
	}
	return 0;
}

#endif

