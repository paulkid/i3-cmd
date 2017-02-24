#include <net/if.h> // Must be included first
#include <linux/wireless.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <ifaddrs.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "common.h"

#define INTF_LO 	0
#define INTF_ETH 	1
#define INTF_WL 	2

struct wl_props {
	// Add all relevant/wanted WL data here
	char essid[IW_ESSID_MAX_SIZE + 1];
};

struct eth_props {
	int ifreq_flags;
	// Add all relevant/wanted ETH data here	
};

typedef struct intf_data {

	int type;
	char name[IFNAMSIZ];
	int ifreq_flags;

	union details {
		struct wl_props wlp;
		struct eth_props ethp;
	} details;

} intf_data;



static int default_socket() {
	static int reqsoc = 0;
	
	if ( ! reqsoc) {
		if ((reqsoc = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
			errno_exit("Failed to create default request socket.");
		}
	}

	return reqsoc;
}

static struct ifreq default_ifreq(const char *ifname) {
	struct ifreq ifrq;
	memset(&ifrq, 0, sizeof(ifrq));
	strncpy(ifrq.ifr_name, ifname, IFNAMSIZ);

	return ifrq;
}


static bool intf_flag_request(const char *ifname, const int flag) {
	struct ifreq ifrq = default_ifreq(ifname);
	int reqsoc = default_socket();

	if (ioctl(reqsoc, SIOCGIFFLAGS, &ifrq) == -1)
		errno_exit("Interface flag-check error.");

	return ifrq.ifr_flags & flag;
}


static bool intf_match_type(const char *ifname, int type) {
	struct ifreq ifrq = default_ifreq(ifname);
	int reqsoc = default_socket();

	switch (type) {
		case INTF_LO:
			return intf_flag_request(ifname, IFF_LOOPBACK);

		case INTF_ETH: ; // Yawn...
			struct ethtool_value reqdata;
			reqdata.cmd = ETHTOOL_GLINK;
			ifrq.ifr_data = (char *) &reqdata;
			return ioctl(reqsoc, SIOCETHTOOL, &ifrq) != -1;

		case INTF_WL:
			return ioctl(reqsoc, SIOCGIWNAME, &ifrq) != -1;
	}
}


/* Populates the struct wl_props with the desired information. */
static struct wl_props get_wl_props(const char *ifname) {
	struct wl_props wlp;

	struct iwreq iwrq;
	memset(&iwrq, 0, sizeof(iwrq));
	strncpy(iwrq.ifr_ifrn.ifrn_name, ifname, IFNAMSIZ);

	iwrq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
	iwrq.u.essid.pointer = wlp.essid;

	int recsoc = default_socket();

	if (ioctl(recsoc, SIOCGIWESSID, &iwrq) == -1) 
		errno_exit("Wireless interface ESSID request failed.");

	return wlp;
}

/* Populates the struct eth_props with the desired information. */
static struct eth_props get_eth_props(const char *ifname) {
	struct eth_props ethp;

	struct ifreq ifreq = default_ifreq(ifname);
	int recsoc = default_socket();

	if (ioctl(recsoc, SIOCGIFFLAGS, &ifreq) == -1)
		errno_exit("Interface flag-check failed for eth interface.");

	ethp.ifreq_flags = ifreq.ifr_flags;
}

/*
 * Finds the number of AF_INET type interface.
 */
static int numof_inet_intfs(struct ifaddrs *ifs_head) {
	int count = 0;

	while (ifs_head != NULL) {
		if (ifs_head->ifa_addr->sa_family == AF_INET)
			count++;

		ifs_head = ifs_head->ifa_next;
	}

	return count;
}

static intf_data * get_uniq_intfs_data(struct ifaddrs *ifhead, int num_uniq) {
	struct intf_data *intfdata = (struct intf_data *)
		malloc(sizeof(struct intf_data) * num_uniq);

	int cid = 0; // Current interface data object

	for (struct ifaddrs *head = ifhead; head != NULL; head = head->ifa_next) {

		// Find AF_INET type interfaces only
		if (head->ifa_addr->sa_family != AF_INET)
			continue;


		intf_data intfd;
		char *ifname = head->ifa_name;
		strncpy(intfd.name, ifname, IFNAMSIZ);

		if (intf_match_type(ifname, INTF_ETH)) {
			
			if (head->ifa_flags & IFF_LOOPBACK)
				continue;

			intfd.type = INTF_ETH;
			intfd.details.ethp = get_eth_props(ifname);
			intfdata[cid++] = intfd;

		} 
		else if (intf_match_type(ifname, INTF_WL)) {
			intfd.type = INTF_WL;
			intfd.details.wlp = get_wl_props(ifname);
			intfdata[cid++] = intfd;
		}
	}

	return intfdata;

}

static char * eth_get_block(intf_data intfd) {


	if ( ! intfd.details.ethp.ifreq_flags & IFF_UP ||
	     ! intfd.details.ethp.ifreq_flags & IFF_RUNNING) {
		return EMPTY_BLOCK;
	}

	char *out = (char *) malloc(sizeof(char) * 1000);

	sprintf(out, "  ETH %s", intfd.name);
	return out;
}

static char * wl_get_block(intf_data intfd) {
	if ( ! intf_flag_request(intfd.name, IFF_UP) ||
		 ! intf_flag_request(intfd.name, IFF_RUNNING)) {

		return EMPTY_BLOCK;
	}

	char *out = (char *) malloc(sizeof(char) * 1000);
	sprintf(out, "  %s", intfd.details.wlp.essid);
	return out;
}


char * net_output(int intf_type) {
	struct ifaddrs *ifhead;

	if (getifaddrs(&ifhead))
		errno_exit("Failed to get interface list.");

	int nintfs = numof_inet_intfs(ifhead);
	intf_data *intfdata = get_uniq_intfs_data(ifhead, nintfs);
	freeifaddrs(ifhead);

	// The matching interface
	intf_data intfout = { -1 };

	for (int i = 0; i < nintfs; i++) {
		// Find first interface occurance of matching type.
		if (intfdata[i].type == intf_type) { 
			intfout = intfdata[i];
			break;
		}
	}

	free(intfdata);

	if (intfout.type == -1)
		return EMPTY_BLOCK;

	else switch (intf_type) {
		case INTF_ETH:
			return eth_get_block(intfout);

		case INTF_WL:
			return wl_get_block(intfout);

		default:
			return EMPTY_BLOCK;

	}
}


/* For testing */
//int main( void ) {
//	struct ifaddrs *ifhead;
//	if (getifaddrs(&ifhead))
//		errno_exit("Faied to get interface list.");
//
//	int nintfs = numof_inet_intfs(ifhead);
//	intf_data *intfdata = get_uniq_intfs_data(ifhead, nintfs);
//
//	for (int i = 0; i < nintfs; i++) {
//		printf("%s - ", intfdata[i].name);
//		switch (intfdata[i].type) {
//			case INTF_LO:
//				printf("LOOP"); break;
//			case INTF_WL:
//				printf("WIRELESS (%s)", intfdata[i].details.wlp.essid); break;
//			case INTF_ETH:
//				printf("WIRED (%d)", intfdata[i].details.ethp.ifreq_flags); 
//				break;
//			default:
//				printf("[UNKNOWN]");
//		}
//
//		printf("\n");
//	}
//
//	printf("--------------------------------\n");
//	printf("Found %d interfaces.\n", nintfs);
//
//	for (int i = 0; i < nintfs; i++)
//		if (intfdata[i].type == INTF_WL)
//			free(intfdata[i].details.wlp.essid);
//
//	free(intfdata);
//	freeifaddrs(ifhead);
//}

