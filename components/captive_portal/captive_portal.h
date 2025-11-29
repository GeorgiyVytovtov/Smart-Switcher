#ifndef CAPTIVE_PORTAL_H_
#define CAPTIVE_PORTAL_H_

#include <stdbool.h>
#include <stdio.h>
#include "html_pages.h"


bool captive_portal_is_running(void);
void captive_portal_stop(void);
void captive_portal_start(void);



#endif /* CAPTIVE_PORTAL_H_ */