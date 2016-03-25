#ifndef RAFTABLE_WORKER_H
#define RAFTABLE_WORKER_H

#include <limits.h>

#include "raft.h"

#define RAFTABLE_PEERS_MAX (64)

typedef struct HostPort {
	bool up;
	char host[HOST_NAME_MAX + 1];
	int port;
} HostPort;

typedef void *(*StateGetter)(void);

typedef struct WorkerConfig {
	int id;
	raft_config_t raft_config;
	HostPort peers[RAFTABLE_PEERS_MAX];
	StateGetter getter;
} WorkerConfig;

void worker_register(WorkerConfig *cfg);
void parse_peers(HostPort *peers, char *peerstr);

#endif