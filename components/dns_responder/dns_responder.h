#ifndef DNS_RESPONDER_H_
#define DNS_RESPONDER_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdbool.h>

bool dns_responder_is_running(void);
void dns_responder_start(void);
void dns_responder_stop(void);

#endif /* DNS_RESPONDER_H_ */