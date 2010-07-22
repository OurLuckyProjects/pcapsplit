//  Copyright (C) 2010 Lothar Braun <lothar@lobraun.de>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "connection.h"

#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <stdlib.h>
#include <string.h>
#include <tools/msg.h>

#include <arpa/inet.h>
		
struct connection_pool_t {
	struct connection* pool;
	list_t* free_list;
	list_t* used_list;

	uint32_t pool_size;
	uint32_t max_pool_size;
	uint32_t timeout;
};

struct connection_pool_t connection_pool;
struct connection*  connections = NULL;
struct connection lookup_conn;
struct connection* found_conn;

int key_fill(record_key_t* key, const struct packet* p)
{
	memset(key, 0, sizeof(*key));
	if (p->is_ip) {
		uint16_t sport, dport;
		sport = dport = 0;
		struct ip* ip = p->ip;

		// TOOD: Handle SCTP. Is this relevant?
		if (ip->ip_p == IPPROTO_TCP) {
			struct tcphdr* tcp = (struct tcphdr*)((uint8_t*)ip + (uint8_t)IP_HDR_LEN(ip));
			sport = tcp->th_sport;
			dport = tcp->th_dport;
		} else if (ip->ip_p == IPPROTO_UDP) {
			struct udphdr* udp = (struct udphdr*)((uint8_t*)ip + (uint8_t)IP_HDR_LEN(ip));
			sport = udp->uh_sport;
			dport = udp->uh_dport;
		}
		if (ip->ip_src.s_addr < ip->ip_dst.s_addr) {
			key->c_v4.ip1 = ip->ip_src.s_addr;
			key->c_v4.ip2 = ip->ip_dst.s_addr;
			key->c_v4.p1  = sport;
			key->c_v4.p2  = dport; 
		} else {
			key->c_v4.ip1 = ip->ip_dst.s_addr;
			key->c_v4.ip2 = ip->ip_src.s_addr;
			key->c_v4.p1  = dport;
			key->c_v4.p2  = sport;
		}
		key->c_v4.proto = ip->ip_p;
		//msg(MSG_ERROR, "%d %d %d %d %d", key->c_v4.ip1, key->c_v4.ip2, key->c_v4.p1, key->c_v4.p2, key->c_v4.proto);
	} else if (p->is_ip6) {
		// TOOD handle IPv6. This is relevant!
	} else {
		// We do not care at the moment
		//msg(MSG_ERROR, "connection_fill: Error, unkonwn packet type");
		return -1;
	}	
	return 0;
}

int connection_init_pool(uint32_t pool_size, uint32_t max_pool_size, uint32_t timeout)
{
	uint32_t i;
	struct connection* c;

	connection_pool.pool_size = pool_size;
	connection_pool.max_pool_size = max_pool_size;
	connection_pool.pool = (struct connection*)malloc(sizeof(struct connection) * pool_size);

	connection_pool.free_list = list_create();
	connection_pool.used_list = list_create();
	
	

	for (i = 0; i != pool_size; ++i) {
		c = &connection_pool.pool[i];
		memset(c, 0, sizeof(struct connection));
		c->element.data = c;
		
		list_push_back(connection_pool.free_list, &c->element);
	}

	return 0;
}

int connection_deinit_pool()
{
	free(connection_pool.pool);
	list_destroy(connection_pool.free_list);
	list_destroy(connection_pool.used_list);
	return 0;
}

struct connection* connection_new(const struct packet* p)
{
	struct list_element_t* t = list_pop_front(connection_pool.free_list);
	struct connection* ret = t->data;
	if (t) {
		list_push_front(connection_pool.used_list, t);
	} else {
		// TODO: handle emtpy pool
	}
	
	return ret;
}

int connection_free(struct connection* c)
{
	HASH_FIND(hh, connections, &c->key, sizeof(record_key_t), found_conn);
	if (found_conn) {
		HASH_DEL(connections, c);
	}
	list_delete_element(connection_pool.used_list, &c->element);
	memset(c, 0, sizeof(*c));
	list_push_back(connection_pool.free_list, &c->element);
	return 0;
}

struct connection* connection_get(const struct packet* p)
{
	key_fill(&lookup_conn.key, p);
	
	HASH_FIND(hh, connections, &lookup_conn.key, sizeof(record_key_t), found_conn);
	if (found_conn) {
		//msg(MSG_ERROR, "Found connection");
	} else {
		msg(MSG_ERROR, "New connection");
		found_conn = connection_new(p);
		key_fill(&found_conn->key, p);
		HASH_ADD(hh, connections, key, sizeof(record_key_t), found_conn);
	}
	return found_conn;
}
