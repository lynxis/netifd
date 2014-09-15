/*
 * netifd - network interface daemon
 * Copyright (C) 2013 Helmut Schaa <helmut.schaa@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "netifd.h"
#include "system.h"
#include "system-ovs.h"

#define run_prog(p, ...) ({ \
        int rc = -1, status; \
        pid_t pid = fork(); \
        if (!pid) \
                exit(execl(p, p, ##__VA_ARGS__, NULL)); \
        if (pid < 0) {\
                rc = -1;\
        } else {\
                while ((rc = waitpid(pid, &status, 0)) == -1 && errno == EINTR); \
		rc = (rc == pid && WIFEXITED(status)) ? WEXITSTATUS(status) : -1; \
        }\
        rc;\
})

static char *system_get_ovs(const char *name)
{
	FILE *f;
	char cmd[64];
	static char dev[64];
	char *c, *c2;

	sprintf(cmd, "/usr/bin/ovs-vsctl iface-to-br %s", name);
	f = popen(cmd, "r");
	if (!f)
		return NULL;
	c = fgets(dev, sizeof(dev), f);
	if (c) {
		c2 = strchr(c, '\n')
		if (c2)
			*c2 = '\0';
	}
	pclose(f);
	return c;
}

static bool system_ovs_isbr(const char *name)
{
	if (run_prog("/usr/bin/ovs-vsctl", "br-exists", name) == 0)
		return true;
	return false;
}

void system_ovs_if_clear_state(struct device *dev)
{
	char *ovs;

	if (system_ovs_isbr(dev->ifname)) {
		system_ovs_delbr(dev);
		return;
	}

	ovs = system_get_ovs(dev->ifname);
	if (ovs)
		run_prog("/usr/bin/ovs-vsctl", "del-port", ovs, dev->ifname);
}


int system_ovs_delbr(struct device *ovs)
{
	if (run_prog("/usr/bin/ovs-vsctl", "del-br", ovs->ifname))
		return -1;
	return 0;
}

int system_ovs_addbr(struct device *ovs, struct ovs_config *cfg)
{
	char buf[16];
	if (cfg->tag && cfg->base) {
		/* Pseudo bridge on top of an openvswitch */
		snprintf(buf, sizeof(buf), "%u", cfg->tag);
		if (run_prog("/usr/bin/ovs-vsctl", "add-br", ovs->ifname, cfg->base, buf))
			return -1;
		return 0;
	}
	if (run_prog("/usr/bin/ovs-vsctl", "add-br", ovs->ifname))
		return -1;
	return 0;
}

int system_ovs_addport(struct device *ovs, struct device *dev)
{
	char *old_ovs;
	system_set_disable_ipv6(dev, "1");

	old_ovs = system_get_ovs(dev->ifname);
	if (old_ovs && !strcmp(old_ovs, ovs->ifname))
		return 0;
	
	if (run_prog("/usr/bin/ovs-vsctl", "add-port", ovs->ifname, dev->ifname))
		return -1;

	return 0;
}

int system_ovs_delport(struct device *ovs, struct device *dev)
{
	system_set_disable_ipv6(dev, "0");
	if (run_prog("/usr/bin/ovs-vsctl", "del-port", ovs->ifname, dev->ifname))
		return -1;
	return 0;
}
