#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>

#include "local.h"
#include "utils.h"
#include "cluster_utils.h"

#define INDEX_PORT 9009

/*
#/var/conf/cluster.conf
MODE=2
VPN_DEV=wan1
TYPE=internet
VIRTUAL_LAN_IP=0.0.0.0/32
VIRTUAL_WAN_MODE=none
VIRTUAL_WAN_IP=0.0.0.0/32
VIRTUAL_WAN_GATEWAY=0.0.0.0
HB_IP=0.0.0.0
DOWN_TIME=30
 */
short g_mode=CLUSTER_MODE_0;
short g_type=CLUSTER_TYPE_NONE;
char  g_line_select[24];	/* line,device */
int   g_virtual_lan_enable;
char  g_virtual_lan_ip[24];
int	  g_virtual_wan_enable;
char  g_virtual_wan_ip[24];
char  g_virtual_wan_gateway[24];
int	  g_virtual_wan1_enable;
char  g_virtual_wan1_ip[24];
char  g_virtual_wan1_gateway[24];
char  g_hb_ip[24];
int   g_down_time=MIN_DOWN_TIME;

char   g_wan_dev[8];
char   g_wan1_dev[8];
char   g_lan_dev[8];
char   g_wan_mac[24];
char   g_wan1_mac[24];
char   g_lan_mac[24];


static int ip_add_boot_wan();
static int ip_add_boot_lan();
static int ip_add_static_route();

static char dynvpn_index[LineMaxLen];
static int dynvpn_port=INDEX_PORT;

static int dynvpn_index_read(void)
{
	char  filename[255];
	FILE  *file=NULL;
	char tmpbuf[LineMaxLen];
	char tmpbuf1[LineMaxLen];

	bzero(tmpbuf, LineMaxLen);

	bzero(filename, 255);
	strncpy(filename, INDEX_CONF_FILE, 255);
	file=fopen(filename,"r");
	if(file==NULL){
#ifdef DEBUG
		perror("read file error");
#endif
		return(-1);
	}

	get_profile(file, '=', "LIST1", tmpbuf);

	fclose(file);

	bzero(dynvpn_index, LineMaxLen);
	bzero(tmpbuf1, LineMaxLen);

	get_section_value(tmpbuf, ':', 1, dynvpn_index, sizeof(dynvpn_index));
	get_section_value(tmpbuf, ':', 2, tmpbuf1, sizeof(tmpbuf1));

	dynvpn_port=atoi(tmpbuf1);
	if (0==dynvpn_port) dynvpn_port=INDEX_PORT;

	return(0);

}

static int open_socket(int timeout)
{
	struct  hostent         *he;
	struct  sockaddr_in     their_addr ;    /* connector's address information */
	int sockfd=0;

	dynvpn_index_read();

	/* get the host info */
	if ((he=gethostbyname(dynvpn_index)) == NULL) {
		dbg("can not get host by name: %m");
		return(-1);
	}

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		dbg("can not set up socket: %m");
		return(-1);
	}

	their_addr.sin_family = AF_INET;      /* host byte order */
	their_addr.sin_port = htons(dynvpn_port);    /* short, network byte order */
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(their_addr.sin_zero), 8);     /* zero the rest of the struct */

	//      signal(SIGALRM, connect_alarm); 

	dbg("open_socket %s:%d timeout:%d\n", dynvpn_index, dynvpn_port, timeout);
	alarm(timeout);
	if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) < 0 ) {
		dbg("can not connect socket: %m");

		//              syslog(LOG_DEBUG, "connect < 0 ; errno=%d\n", errno);
		close(sockfd);
		if ( errno == EINTR ) {
			errno = ETIMEDOUT;
			printf("Connect Timed out\n");
			syslog(LOG_DEBUG, "Connect Timed out\n");
		}
		return(-1);
	}
	alarm(0);

	return sockfd;
}

/* **********************************
   int readable_timeo(int fd, int sec)
   Return Value:
        -1 :  error
         0 :  timedout
        >0 :  descriptor is readable
 * ******************************** */
int readable_timeo(int fd, int sec)
{
        fd_set  rset;
        struct timeval tv;

        FD_ZERO(&rset);
        FD_SET(fd, &rset);

        tv.tv_sec = sec;
        tv.tv_usec = 0;

        return(select(fd+1, &rset, NULL, NULL, &tv));
}

int get_dev_info(const char *conf_file)
{
	FILE *file=NULL;

	bzero(g_lan_dev, 8);
	bzero(g_wan_dev, 8);
	bzero(g_wan1_dev, 8);
	bzero(g_lan_mac, 24);
	bzero(g_wan_mac, 24);
	bzero(g_wan1_mac, 24);

	if ( NULL == (file=fopen(conf_file, "r")) ) return(-1);
	get_profile(file, '=', "LAN_DEV", g_lan_dev);
	get_profile(file, '=', "WAN_DEV", g_wan_dev);
	get_profile(file, '=', "WAN1_DEV", g_wan1_dev);

	fclose(file);

	// LEOLEO
	get_mac(g_lan_dev, g_lan_mac, sizeof(g_lan_mac));
	get_mac(g_wan_dev, g_wan_mac, sizeof(g_wan_mac));
	get_mac(g_wan1_dev, g_wan1_mac, sizeof(g_wan1_mac));

	return(0);
}

int cluster_read(const char *conf_file)
{
	char logbuf[LineMaxLen];
	FILE *file=NULL;

	char buf[LineMaxLen];

	if ( NULL == (file=fopen(conf_file, "r")) ) return(-1);
	// MODE
	bzero(buf, LineMaxLen);
	get_profile(file, '=', "MODE", buf);
	g_mode=atoi(buf);

	if ( g_mode > 3 || g_mode < 0 ) {
		bzero(logbuf, LineMaxLen);
		sprintf(logbuf, "cluster_read: invalid cluster mode:%s", buf);
		do_log(logbuf);
		fclose(file);
		return(1);	
	}

	// TYPE
	bzero(buf, LineMaxLen);
	get_profile(file, '=', "TYPE", buf);
	if ( streq(buf, "vpn") ) g_type=CLUSTER_TYPE_VPN;
	else if ( streq(buf, "internet") ) g_type=CLUSTER_TYPE_INTERNET;
	else if ( streq(buf, "master") ) g_type=CLUSTER_TYPE_MASTER;
	else if ( streq(buf, "backup") ) g_type=CLUSTER_TYPE_BACKUP;
	else {
		bzero(logbuf, LineMaxLen);
		sprintf(logbuf, "cluster_read: invalid cluster type:%s", buf);
		do_log(logbuf);
		fclose(file);
		return(2);	
	}

	// LINE_SELECT
	bzero(g_line_select, 24);
	get_profile(file, '=', "LINE_SELECT", g_line_select);

	g_virtual_lan_enable = enable_check(file, "VIRTUAL_LAN_ENABLE", NO);
	if ( g_virtual_lan_enable ) {
		// VIRTUAL_LAN_IP
		bzero(g_virtual_lan_ip, 24);
		get_profile(file, '=', "VIRTUAL_LAN_IP", g_virtual_lan_ip);
		if ( !valid_ip_subnet(g_virtual_lan_ip, NO_NET_FORMAT) ) {
			bzero(logbuf, LineMaxLen);
			sprintf(logbuf, "cluster_read: invalid virtual_lan_ip :%s", g_virtual_lan_ip);
			do_log(logbuf);
			fclose(file);
			return(3);	
		}
	}

	// WAN
	g_virtual_wan_enable = enable_check(file, "VIRTUAL_WAN_ENABLE", NO);
	if ( g_virtual_wan_enable ) {
		// VIRTUAL_WAN_IP
		bzero(g_virtual_wan_ip, 24);
		get_profile(file, '=', "VIRTUAL_WAN_IP", g_virtual_wan_ip);

		// VIRTUAL_WAN_GATEWAY
		bzero(g_virtual_wan_gateway, 24);
		get_profile(file, '=', "VIRTUAL_WAN_GATEWAY", g_virtual_wan_gateway);
	}

	// WAN1
	g_virtual_wan1_enable = enable_check(file, "VIRTUAL_WAN1_ENABLE", NO);
	if ( g_virtual_wan1_enable ) {
		// VIRTUAL_WAN1_IP
		bzero(g_virtual_wan1_ip, 24);
		get_profile(file, '=', "VIRTUAL_WAN1_IP", g_virtual_wan1_ip);

		// VIRTUAL_WAN1_GATEWAY
		bzero(g_virtual_wan1_gateway, 24);
		get_profile(file, '=', "VIRTUAL_WAN1_GATEWAY", g_virtual_wan1_gateway);
	}


	// HB_IP
	bzero(g_hb_ip, 24);
	get_profile(file, '=', "HB_IP", g_hb_ip);
	if ( !valid_ip(g_hb_ip) ) {
		bzero(logbuf, LineMaxLen);
		sprintf(logbuf, "cluster_read: invalid g_hb_ip :%s", g_hb_ip);
		do_log(logbuf);
		fclose(file);
		return(6);	
	}

	// DOWN_TIME
	bzero(buf, LineMaxLen);
	get_profile(file, '=', "DOWN_TIME", buf);
	g_down_time=atoi(buf);

	if ( g_down_time < MIN_DOWN_TIME ) {
		bzero(logbuf, LineMaxLen);
		sprintf(logbuf, "cluster_read: invalid down_time :%s Min=%ds", buf, MIN_DOWN_TIME);
		do_log(logbuf);
		fclose(file);
		return(7);	
	}	

	fclose(file);

	if (debug) {
		printf("cluster_read():\n");
		printf("  cluster settings:\n");
		printf("     mode              : %d\n", g_mode);
		printf("     type              : %d \n", g_type);
		printf("             CLUSTER_TYPE_NONE      %d\n", CLUSTER_TYPE_NONE);
		printf("             CLUSTER_TYPE_INTERNET  %d\n", CLUSTER_TYPE_INTERNET);
		printf("             CLUSTER_TYPE_VPN       %d\n", CLUSTER_TYPE_VPN);
		printf("             CLUSTER_TYPE_MASTER    %d\n", CLUSTER_TYPE_MASTER);
		printf("             CLUSTER_TYPE_BACKUP    %d\n", CLUSTER_TYPE_BACKUP);
		printf("     virtual_lan_enable : %d\n", g_virtual_lan_enable); 
		if ( g_virtual_lan_enable ) {
			printf("     virtual_lan_ip     : %s\n", g_virtual_lan_ip); 
		}
		printf("     virtual_wan_enable : %d\n", g_virtual_wan_enable); 
		if ( g_virtual_wan_enable ) {
			printf("     virtual_wan_ip     : %s\n", g_virtual_wan_ip); 
			printf("     virtual_wan_gateway: %s\n", g_virtual_wan_gateway); 
		}
		printf("     virtual_wan1_enable : %d\n", g_virtual_wan1_enable); 
		if ( g_virtual_wan1_enable ) {
			printf("     virtual_wan1_ip     : %s\n", g_virtual_wan1_ip); 
			printf("     virtual_wan1_gateway: %s\n", g_virtual_wan1_gateway); 
		}
		printf("     hb_ip              : %s\n", g_hb_ip); 
		printf("     down_time (s)      : %d\n", g_down_time); 
	}

	return(0);
}

void do_quit()  
{       
	syslog(LOG_INFO, "......Quit!\n");
	exit(0);
}

void do_log(const char *logbuf)
{
	if ( debug ) printf("%s\n", logbuf);
	syslog(LOG_INFO, "%s", logbuf);
}


////////////////////
// Get value from 
// Getting WAN stats from POLICYROUTER_RUNNING
int wan_check(int wan_type)
{
	FILE *file=NULL;
	char wan_status[8];
	char key[LineMaxLen];

	if ( NULL != (file=fopen(POLICYROUTER_RUNNING, "r")) ) {
		bzero(wan_status, 8);

		bzero(key, LineMaxLen);

		if ( WAN == wan_type ) strcpy(key, "WAN_STATUS");
		else if ( WAN1 == wan_type ) strcpy(key, "WAN1_STATUS");
		else if ( WAN2 == wan_type ) strcpy(key, "WAN2_STATUS");
		else if ( WAN3 == wan_type ) strcpy(key, "WAN3_STATUS");
		else strcpy(key, "WAN_STATUS");

		get_profile(file, '=', key, wan_status);

		fclose(file);

		if ( 0 == strcmp(wan_status, "UP") ) return(UP_STATUS);
		else return(DOWN_STATUS);
	}

	return(DOWN_STATUS);
}




/* ********************************************
   ip_check(const char *dev, const char *ip);
Desc:  Check whether if lan_ip is existed
Return Value:
0:  ip is not existed
1:  ip is existed
 * ***************************************** */
int ip_check(const char *dev, const char *ip)
{
	FILE *file=NULL;
	char cmd[255];
	char tmpbuf[200];
	int  ip_exist=0;

	if ( streq(ip, "0.0.0.0/32") ) return(0);

	bzero(cmd, 255);
	sprintf(cmd, "%s addr show dev %s | grep %s", IP_CMD, dev, ip);

	if (debug) printf("ip_check: %s\n", cmd);
	if ( NULL == (file=popen(cmd, "r")))
		bzero(tmpbuf, 200);
	while ( NULL != fgets(tmpbuf, 200, file)) {
		if ( NULL != strstr(tmpbuf, ip)) {
			ip_exist=1;
			break;
		}
		bzero(tmpbuf, 200);
	}
	pclose(file);

	if (debug) printf("ip_check: ip_exist=%d\n", ip_exist);

	if ( ip_exist ) return(1);
	else return(0);
}

int route_check(const char *dst, const char *nexthop)
{
	FILE *pfile=NULL;
	char cmd[LineMaxLen];
	char buf[LineMaxLen];
	int existed=0;

	char m_dst[24];
	char m_via[24];
	char m_nexthop[24];

	bzero(cmd, LineMaxLen);
	sprintf(cmd, "%s route | grep via | grep %s", IP_CMD, dst);
	if ( NULL == (pfile=popen(cmd, "r"))) return(0);

	existed=0;
	bzero(buf, LineMaxLen);
	while ( NULL != fgets(buf, LineMaxLen, pfile) ) {
		bzero(m_dst, 24);
		bzero(m_via, 24);
		bzero(m_nexthop, 24);

		get_section_value(buf, ' ', 1, m_dst, sizeof(m_dst));
		get_section_value(buf, ' ', 2, m_via, sizeof(m_via));
		get_section_value(buf, ' ', 3, m_nexthop, sizeof(m_nexthop));

		if ( streq(m_dst, dst) && streq(m_via, "via") && streq(m_nexthop, nexthop)) {
			existed=1;
			break;
		}
		bzero(buf, LineMaxLen);
	}
	pclose(pfile);

	if (debug) printf("route_check(%s,%s) : existed=%d\n", dst, nexthop, existed);

	return existed;
}

void route_add(const char *dst, const char *nexthop)
{
	char cmd[255];

	bzero(cmd, 255);

	sprintf(cmd, "%s route add %s via %s > /dev/null 2>&1", IP_CMD, dst, nexthop);
	if (debug) {
		printf("ip_route: %s\n", cmd);
		printf("%s: added\n",cmd);
	}
	//dbglog("%t - route_add(dst[%s], nexthop[%s]), cmd[%s]", dst, nexthop, cmd);
	system(cmd);

	return;
}

int vpn_check()
{
	//if ( is_regular_file(LICENSE_FILE) ) return(INACTIVE_STATUS);

	if (is_running(DAEMON_PID_FILE, VPN_DAEMON_CMD)) {
		if ( is_regular_file(INDEXDIGITAL_FAILED_FILE) ) {
			return(INDEX_DIGITAL_FAILED_STATUS);              
		} else {
			return(ACTIVE_STATUS);              
		}
	} else {                                               
		return(INACTIVE_STATUS);              
	}

	return(INACTIVE_STATUS);              
}

short cluster_check(HB_PACKET *hb)
{
	/*
	   typedef struct _hb_packet {
	   uint8 msg_type;
	   uint8 cluster_id;
	   uint8 cluster_mode;
	   uint8 cluster_type;
	   uint8 wan_status;
	   uint8 virtual_wan_status;
	   uint8 virtual_lan_status;
	   uint8 vpn_status;

	///////////////////
	uint8 wan1_status;
	uint8 virtual_wan1_status;
	uint8 wan2_status;
	uint8 virtual_wan2_status;
	uint8 wan3_status;
	uint8 virtual_wan3_status;
	uint8 virtual_lan1_status;
	uint8 virtual_func_status;

	uint8 reserver[8];      // reserver[16]
	} HB_PACKET;
	 */

	// wan_status
	hb->wan_status=wan_check(WAN);
	hb->wan1_status=wan_check(WAN1);

	// virtual_wan_status
	//if ( ip_check(g_wan_dev, g_virtual_wan_ip) && route_check("default", g_virtual_wan_gateway) ) hb->virtual_wan_status=ACTIVE_STATUS;
	if ( hb->wan_status && g_virtual_wan_enable && ip_check(g_wan_dev, g_virtual_wan_ip) ) hb->virtual_wan_status=ACTIVE_STATUS;
	else hb->virtual_wan_status=INACTIVE_STATUS;
	// virtual_wan1_status
	if ( hb->wan1_status && g_virtual_wan1_enable && ip_check(g_wan1_dev, g_virtual_wan1_ip) ) hb->virtual_wan1_status=ACTIVE_STATUS;
	else hb->virtual_wan1_status=INACTIVE_STATUS;
	// virtual_lan_status
	if ( g_virtual_lan_enable && ip_check(g_lan_dev, g_virtual_lan_ip) ) hb->virtual_lan_status=ACTIVE_STATUS;
	else hb->virtual_lan_status=INACTIVE_STATUS;
	// vpn_status
	hb->vpn_status=vpn_check();

	return(0);
}


/* add ip */
int ip_add(const char *dev, const char *ip)
{
	char cmd[255];

	//dbglog("ip_add(dev[%s], ip[%s]) - %t", dev, ip);

	if ( streq(ip, "0.0.0.0/32") ) return(1);

	if ( streq(g_wan_dev, dev) ) {
		FILE *file=NULL;
		char wan_mode[16];
		char wan_ip[24];
		char wan_netmask[24];
		int  wan_netmask_len=0;

		char wan_ipnet[24];

		if (debug) {
			printf("Adding virtual wan ip\n");
		}
		// 1. flush wan
		bzero(cmd, 255);
		sprintf(cmd, "%s addr flush dev %s > /dev/null 2>&1", IP_CMD, dev);
		if (debug) printf("%s\n", cmd);
		if ( 0 != system(cmd) ) {
			if (debug) {
				printf("Flush dev %s, failed!\n", dev);
			}
			return(-1);
		}
		if (debug) {
			printf("Flush dev %s, OK\n", dev);
		}

		// 2. add vwan as main ip
		bzero(cmd, 255);
		sprintf(cmd, "%s addr add %s dev %s > /dev/null 2>&1", IP_CMD, ip, dev);
		if (debug) printf("%s\n", cmd);
		if ( 0 != system(cmd) ) {
			if (debug) {
				printf("Adding virtual wan %s to dev %s, failed!\n", ip, dev);
			}
			return(-1);
		}
		if (debug) {
			printf("Adding virtual wan %s to dev %s, OK!\n", ip, dev);
		}

		// 3. add wan_ip if existed
		bzero(wan_mode, 16);
		bzero(wan_ip, 24);
		bzero(wan_netmask, 24);

		/*
		   WAN_MODE=ddn
		   DDN_IP=10.1.0.11
		   DDN_NETMASK=255.255.255.0
		 */
		if ( NULL != (file=fopen(PROFILE_FILE, "r")) ) {
			get_profile(file, '=', "WAN_MODE", wan_mode);
			get_profile(file, '=', "DDN_IP", wan_ip);
			get_profile(file, '=', "DDN_NETMASK", wan_netmask);

			fclose(file);
		}

		wan_netmask_len=valid_netmask(wan_netmask);

		bzero(wan_ipnet, 24);
		sprintf(wan_ipnet, "%s/%d", wan_ip, wan_netmask_len);

		if ( 0 == strncmp(wan_mode, "ddn", 3) && valid_ip(wan_ip) && 0 != strcmp(wan_ip, "0.0.0.0") && wan_netmask_len ) {
			if ( ! streq(wan_ipnet, ip) ) {
				bzero(cmd, 255);
				sprintf(cmd, "%s addr add %s/%d dev %s > /dev/null 2>&1", IP_CMD, wan_ip, wan_netmask_len, dev);
				if (debug) printf(cmd);
				if ( 0 != system(cmd) ) {
					if (debug) {
						printf("Adding wan %s/%d to dev %s, failed!\n", wan_ip, wan_netmask_len, dev);
					}
					return(-1);
				}
				if (debug) {
					printf("Adding wan %s/%d to dev %s, OK!\n", wan_ip, wan_netmask_len, dev);
				}
			}
		}

		// 4. add multiple wan_ip if existed
		ip_add_boot_wan();

		ip_add_static_route();

	} else {
		bzero(cmd, 255);
		sprintf(cmd, "%s addr add %s dev %s > /dev/null 2>&1", IP_CMD, ip, dev);
		if (debug) {
			printf("ip_add: %s\n", cmd);
			printf("Ip %s at dev %s added\n",ip, dev);
		}

		system(cmd);
		ip_add_boot_lan();
		ip_add_static_route();
	}
	return(0);
}

int ip_del(const char *dev, const char *ip)
{
	char cmd[255];

	if (debug) {
		printf("ip_del(dev[%s], ip[%s])\n", dev, ip);
	}
	//dbglog("%t - ip_del(dev[%s], ip[%s])\n", dev, ip);

	if ( streq(ip, "0.0.0.0/32") ) return(0);

	if ( streq(g_wan_dev, dev) ) {
		// WAN operation
		FILE *file=NULL;
		char wan_mode[16];
		char wan_ip[24];
		char wan_netmask[24];
		int  wan_netmask_len=0;

		char wan_ipnet[24];

		if (debug) {
			printf("Del virtual wan ip\n");
		}

		// 1. check wan_ip 
		bzero(wan_mode, 16);
		bzero(wan_ip, 24);
		bzero(wan_netmask, 24);

		if ( NULL != (file=fopen(PROFILE_FILE, "r")) ) {
			get_profile(file, '=', "WAN_MODE", wan_mode);
			get_profile(file, '=', "DDN_IP", wan_ip);
			get_profile(file, '=', "DDN_NETMASK", wan_netmask);

			fclose(file);
		}

		wan_netmask_len=valid_netmask(wan_netmask);

		bzero(wan_ipnet, 24);
		sprintf(wan_ipnet, "%s/%d", wan_ip, wan_netmask_len);
		// When ip_del ip = wan ip, just remove it 

		// 2. flush wan
		bzero(cmd, 255);
		sprintf(cmd, "%s addr flush dev %s > /dev/null 2>&1", IP_CMD, dev);
		if (debug) printf(cmd);
		if ( 0 != system(cmd) ) {
			if (debug) {
				printf("Flush dev %s, failed!\n", dev);
			}
			return(-1);
		}
		if (debug) {
			printf("Flush dev %s, OK\n", dev);
		}

		if ( 0 == strncmp(wan_mode, "ddn", 3) && valid_ip(wan_ip) && 0 != strcmp(wan_ip, "0.0.0.0") && wan_netmask_len ) {
			if ( !streq(wan_ipnet, ip) ) {
				bzero(cmd, 255);
				sprintf(cmd, "%s addr add %s/%d dev %s > /dev/null 2>&1", IP_CMD, wan_ip, wan_netmask_len, dev);
				if (debug) printf(cmd);
				if ( 0 != system(cmd) ) {
					if (debug) {
						printf("Adding wan %s/%d to dev %s, failed!\n", wan_ip, wan_netmask_len, dev);
					}
					return(-1);
				}
				if (debug) {
					printf("Adding wan %s/%d to dev %s, OK!\n", wan_ip, wan_netmask_len, dev);
				}
			}
		}

		// 4. add multiple wan_ip if existed
		ip_add_boot_wan();
		ip_add_static_route();

	} else {

		bzero(cmd, 255);

		sprintf(cmd, "%s addr del %s dev %s > /dev/null 2>&1", IP_CMD, ip, dev);
		if (debug) printf("ip %s at dev %s deleted\n",ip, dev);

		system(cmd);

		ip_add_boot_lan();
		ip_add_static_route();
	}

	return(0);
}

int ip_mac_broadcast(const char *dev, char *ip_addr, char *mac_addr)
{
	// ip_addr: 182.15.0.3/23 -> ipaddr : 182.15.0.3
	char cmd[255];
	char ipaddr[20];
	char *p=NULL;

	bzero(cmd, 255);
	bzero(ipaddr, 20);

	if ( NULL == (p=strchr(ip_addr, '/'))) return(1);
	strncpy(ipaddr, ip_addr, p-ip_addr);

	//sprintf(cmd, "%s %s %s %s ff:ff:ff:ff:ff:ff %s", SEND_ARP, ipaddr, mac_addr, ipaddr, dev);
	sprintf(cmd, "%s %s %s %s", SEND_ARP, ipaddr, mac_addr, dev);

	printf("Broadcast ip %s ,mac %s, dev %s\n", ipaddr, mac_addr, dev);
	//dbglog("%t - Broadcast ip %s ,mac %s, dev %s\n", ipaddr, mac_addr, dev);


	if (debug) printf("%s\n", cmd);
	return system(cmd);
}

static int ip_add_static_route()
{
	char cmd[255];

	bzero(cmd, 255);

	sprintf(cmd, "%s > /dev/null 2>&1", BOOT_ROUTE_FILE);

	return system(cmd);
}

static int ip_add_boot_wan()
{
	char cmd[255];

	bzero(cmd, 255);
	sprintf(cmd, "%s > /dev/null 2>&1", BOOT_WAN_FILE);
	if (debug) printf("%s\n", cmd);
	return system(cmd);
}

static int ip_add_boot_lan()
{
	char cmd[255];

	bzero(cmd, 255);
	sprintf(cmd, "%s > /dev/null 2>&1", BOOT_LAN_FILE);
	if (debug) printf("%s\n", cmd);
	return system(cmd);
}

int client_reset_group(int type, int group)
{
	FILE *file=NULL;
	char enable[16];
	char groupname[16];
	char nodename[16];
	char license[32];

	//LICENSE_ENABLE
	char key_enable[16];
	char key_groupname[16];
	char key_nodename[16];
	char key_license[32];
	char str_group[32];
	int m_sockfd;

	char sendbuf[100];
	char recvbuf[100];
	int n_byte;

	bzero(enable, sizeof(enable));
	bzero(groupname, sizeof(groupname));
	bzero(nodename, sizeof(nodename));
	bzero(license, sizeof(license));

	bzero(key_enable, sizeof(key_enable));
	bzero(key_groupname, sizeof(key_groupname));
	bzero(key_nodename, sizeof(key_nodename));
	bzero(key_license, sizeof(key_license));
	bzero(str_group, sizeof(str_group));

	if ( 0 == group ) {
		bzero(str_group, sizeof(str_group));
	} else if ( group > 0 && group < 4 ) {
		sprintf(str_group, "%d", group);
	} else {
		return(0);
	}

	sprintf(key_enable, "LICENSE%s_ENABLE", str_group);
	sprintf(key_groupname, "GROUPNAME%s", str_group);
	sprintf(key_nodename, "NODENAME%s", str_group);
	sprintf(key_license, "LICENSE%s", str_group);

	switch (type ) {
		case CLUSTER_TYPE_INTERNET:    break;
		case CLUSTER_TYPE_VPN:    break;
		case CLUSTER_TYPE_MASTER:    break;
		case CLUSTER_TYPE_BACKUP:    break;
		default:    
									 if (debug) {
										 printf("client_reset(%d) group(%d): type error.\n", type, group);
										 syslog(LOG_DEBUG, "client_reset(%d) group(%d): type error.\n", type, group);
									 }
									 return(1);

									 break;
	}

	if ( CLUSTER_TYPE_INTERNET == type ) printf("INTERNET ");
	else if ( CLUSTER_TYPE_VPN == type ) printf("VPN ");
	else if ( CLUSTER_TYPE_MASTER == type ) printf("MASTER ");
	else if ( CLUSTER_TYPE_BACKUP == type ) printf("BACKUP ");
	else return(-1);

	if ( NULL == (file=fopen(LICENSE_FILE,  "r")) ) return(1);
	get_profile(file, '=', key_enable, enable);
	get_profile(file, '=', key_groupname, groupname);
	get_profile(file, '=', key_nodename, nodename);
	get_profile(file, '=', key_license, license);

	fclose(file);

	if ( !streq(enable, "yes") ) return(2);
	if ( streq(groupname, "none") ) return(3);

	//	printf("Into client_reset(%d)\n", type);
	//dbglog("%t - Into client_reset(%d)", type);

	bzero(sendbuf, 100);
	sendbuf[0]=Req_Client_Reset;
	strncpy(sendbuf+1, groupname, 8);
	strncpy(sendbuf+9, nodename, 8);
	strncpy(sendbuf+17, license, 16);
	sendbuf[33]=type;
	sendbuf[34]='\n';

	m_sockfd = open_socket(5);

	//dbglog("%t client_reset(%d) after open_socket()", type);

	if ( m_sockfd <= 0 ) {
		printf(" open_socket error[%d]\n", m_sockfd);
		//dbglog("client_reset open_socket error[%d]\n", m_sockfd);
		return(-1);
	}

	if (send(m_sockfd, sendbuf, 34, 0) == -1) {
		printf("send error\n");
		close(m_sockfd);
		return(-1);
	}
	//dbglog("%t client_reset(%d) after send()", type);

	////////////
	if ( 0 == readable_timeo(m_sockfd, 5) ) {
		//dbglog("%t client_reset(%d) after readable_timo()==0", type);
		close(m_sockfd);
		return(-1);
	} else {
		bzero(recvbuf, 100);
		n_byte=recv(m_sockfd, recvbuf, 100, 0);
		//dbglog("%t client_reset(%d) after recv()==0", type);
		if ( -1 == n_byte ) {
			close(m_sockfd);
			return(-1);
		}
	} // end -- if ()
	/////////////

	close(m_sockfd);

	if ( CLUSTER_TYPE_INTERNET == type ) printf("INTERNET");
	else if ( CLUSTER_TYPE_VPN == type ) printf("VPN");
	else if ( CLUSTER_TYPE_MASTER == type ) printf("MASTER");
	else if ( CLUSTER_TYPE_BACKUP == type ) printf("BACKUP");
	else return(-1);

	printf(" : Client reset done!\n");
	//dbglog(" : Client reset done! - %t\n");

	return(0);
}

int client_reset(int type)
{
	syslog(LOG_DEBUG,"client_reset(%d)", type);
	client_reset_group(type, 0);
	client_reset_group(type, 1);
	client_reset_group(type, 2);
	client_reset_group(type, 3);
	return(0);
}

int ipsec_shutdown()
{
	char cmd[256];
	bzero (cmd, 256);

	sprintf (cmd, "%s setup stop >/dev/null 2>&1", IPSEC_CMD);

	return system(cmd);
}

/* ***********************************************************
   int vpn_daemon_check()
Desc: check whether vpn_daemon is running or correctly
Return Values: 
0:  vpn_daemon() is running correctly
1:  vpn_daemon() is not running
2:  vpn_daemon() is not running correctly
 * ********************************************************* */
int vpn_daemon_check()
{
	if (is_running(DAEMON_INIT_PID_FILE, VPN_DAEMON_CMD)) {
		return(2);
	} else if (is_running(DAEMON_PID_FILE, VPN_DAEMON_CMD)) {
		// Check whether its digital sige is OK

	} else {
		return(1);
	}

	return(0);
}

int do_vpn_stop()
{
	int pid=0;
	int Ret=0;

	if (is_running(DAEMON_PID_FILE, VPN_DAEMON_CMD)) {
		pid=get_pid_from_file(DAEMON_PID_FILE);
	} else {
		return(1);
	}

	if (pid > 0) {
		Ret =kill(pid, SIGKILL);
		if (Ret == -1) {
			return(-1);
		} else if (Ret == 0) {
			ipsec_shutdown();
		}
	}
	printf("vpn stop ok!\n");

	return(0);
}

int do_vpn_start()
{
	char cmd[255];
	bzero(cmd, 255);

	sprintf(cmd, "%s > /dev/null 2>&1", VPN_DAEMON_CMD);

	printf("vpn daemon start !\n");
	return system(cmd);
}

int do_vpn_restart()
{
	do_vpn_stop();
	sleep(1);
	do_vpn_start();
	return(0);
}

int save_status(HB_PACKET *hb, int alive_status)
{
	/*
	   ALIVE=up|down
	   WAN=up|down
	   VWAN=up|down
	   VLAN=up|down
	   VPN=up|down
	 */
	char logbuf[LineMaxLen];
	FILE *file=NULL;

	char filename[255];
	char lockfilename[255];
	char name[100];

	bzero(filename, 255);
	bzero(lockfilename, 255);
	bzero(name, 100);

	if ( CLUSTER_TYPE_VPN == hb->cluster_type ) {
		sprintf(filename, "%s.vpn", CLUSTER_RUNNING);
		sprintf(lockfilename, "%s.vpn.lock", CLUSTER_RUNNING);
		strcpy(name, "VPN");
	} else if ( CLUSTER_TYPE_INTERNET == hb->cluster_type ) {
		sprintf(filename, "%s.internet", CLUSTER_RUNNING);
		sprintf(lockfilename, "%s.internet.lock", CLUSTER_RUNNING);
		strcpy(name, "INTERNET");
	} else if ( CLUSTER_TYPE_MASTER == hb->cluster_type ) {
		sprintf(filename, "%s.master", CLUSTER_RUNNING);
		sprintf(lockfilename, "%s.master.lock", CLUSTER_RUNNING);
		strcpy(name, "MASTER");
	} else if ( CLUSTER_TYPE_BACKUP == hb->cluster_type ) {
		sprintf(filename, "%s.backup", CLUSTER_RUNNING);
		sprintf(lockfilename, "%s.backup.lock", CLUSTER_RUNNING);
		strcpy(name, "BACKUP");
	} else {
		if (debug) {
			bzero(logbuf, LineMaxLen);
			sprintf(logbuf, "save_status() invalid cluster_type=[%d].\n", hb->cluster_type);
			do_log(logbuf);
		}
		return(1);
	}

	touch_file(lockfilename);

	if ( NULL != (file=fopen(filename, "w")) ) {
		fprintf(file, "== Cluster type: %s ====\n", name);
		if ( ACTIVE_STATUS==alive_status ) {
			fprintf(file, "ALIVE=%s\n", (ACTIVE_STATUS==alive_status)?"up":"down");
			fprintf(file, "WAN=%s\n", (UP_STATUS==hb->wan_status)?"up":"down");
			fprintf(file, "WAN1=%s\n", (UP_STATUS==hb->wan1_status)?"up":"down");
			fprintf(file, "VWAN=%s\n", (UP_STATUS==hb->virtual_wan_status)?"up":"down");
			fprintf(file, "VWAN1=%s\n", (UP_STATUS==hb->virtual_wan1_status)?"up":"down");
			fprintf(file, "VLAN=%s\n", (UP_STATUS==hb->virtual_lan_status)?"up":"down");
			fprintf(file, "VPN=%s\n", (ACTIVE_STATUS==hb->vpn_status)?"up":"down");
		} else if ( INACTIVE_STATUS==alive_status ) {
			fprintf(file, "ALIVE=down\n");
			fprintf(file, "WAN=down\n");
			fprintf(file, "WAN1=down\n");
			fprintf(file, "VWAN=down\n");
			fprintf(file, "VWAN1=down\n");
			fprintf(file, "VLAN=down\n");
			fprintf(file, "VPN=down\n");
		}
		fclose(file);
	}

	unlink(lockfilename);

	if ( debug ) {
		printf("In save_status(): %s\n", filename);
	}

	return(0);
}

/* Return : 0 get value ok
   1 get value failed
 */
int get_status(HB_PACKET *hb, int cluster_type)
{
	char logbuf[LineMaxLen];
	FILE *file=NULL;

	char filename[255];
	char lockfilename[255];
	char value[LineMaxLen];
	int check_times=0;

	bzero(filename, 255);
	bzero(lockfilename, 255);

	if ( CLUSTER_TYPE_VPN == cluster_type ) {
		sprintf(filename, "%s.vpn", CLUSTER_RUNNING);
		sprintf(lockfilename, "%s.vpn.lock", CLUSTER_RUNNING);
	} else if ( CLUSTER_TYPE_INTERNET == cluster_type ) {
		sprintf(filename, "%s.internet", CLUSTER_RUNNING);
		sprintf(lockfilename, "%s.internet.lock", CLUSTER_RUNNING);
	} else if ( CLUSTER_TYPE_MASTER == cluster_type ) {
		sprintf(filename, "%s.master", CLUSTER_RUNNING);
		sprintf(lockfilename, "%s.master.lock", CLUSTER_RUNNING);
	} else if ( CLUSTER_TYPE_BACKUP == cluster_type ) {
		sprintf(filename, "%s.backup", CLUSTER_RUNNING);
		sprintf(lockfilename, "%s.backup.lock", CLUSTER_RUNNING);
	} else {
		if (debug) {
			bzero(logbuf, LineMaxLen);
			sprintf(logbuf, "get_status() invalid cluster_type=[%d].\n", cluster_type);
			do_log(logbuf);
		}
		return(1);
	}

	check_times = 0;
	while ( is_regular_file(lockfilename) ) {
		usleep(100000);
		check_times++;

		if ( check_times > 5 ) {
			return(2);
		}
	}

	bzero(hb, sizeof(HB_PACKET));
	if ( NULL == (file=fopen(filename, "r")) ) {
		return(3);
	}

	// WAN
	bzero(value, LineMaxLen);
	get_profile(file, '=', "WAN", value);
	if ( streq(value, "up") ) {
		hb->wan_status = 1;
	} else {
		hb->wan_status = 0;
	}	
	// WAN1
	bzero(value, LineMaxLen);
	get_profile(file, '=', "WAN1", value);
	if ( streq(value, "up") ) {
		hb->wan1_status = 1;
	} else {
		hb->wan1_status = 0;
	}	
	// VWAN
	bzero(value, LineMaxLen);
	get_profile(file, '=', "VWAN", value);
	if ( streq(value, "up") ) {
		hb->virtual_wan_status = 1;
	} else {
		hb->virtual_wan_status = 0;
	}	
	// VWAN1
	bzero(value, LineMaxLen);
	get_profile(file, '=', "VWAN1", value);
	if ( streq(value, "up") ) {
		hb->virtual_wan1_status = 1;
	} else {
		hb->virtual_wan1_status = 0;
	}	
	// VLAN
	bzero(value, LineMaxLen);
	get_profile(file, '=', "VLAN", value);
	if ( streq(value, "up") ) {
		hb->virtual_lan_status = 1;
	} else {
		hb->virtual_lan_status = 0;
	}	
	// VPN
	bzero(value, LineMaxLen);
	get_profile(file, '=', "VPN", value);
	if ( streq(value, "up") ) {
		hb->vpn_status = 1;
	} else {
		hb->vpn_status = 0;
	}	

	fclose(file);

	if ( debug ) {
		printf("In save_status(): %s\n", filename);
	}

	return(0);
}

void clear_arp_proxy()
{
	char cmd[255];

	bzero(cmd, 255);

	sprintf(cmd, "%s -n | %s -v arp | %s -v Address | %s '{print \"%s -d \"$1}' | /bin/sh", ARP, GREP, GREP, AWK, ARP);

	//dbglog("%t - clear_arp_proxy(cmd[%s])", cmd);

	system(cmd);
}

void virtual_ip_action(int type, int vstatus, char *msg, short opt)
{
	char logbuf[LineMaxLen];
	char l_dev[8];
	char l_virtual_ip[24];
	char l_mac[24];
	char tag[8];

	memset(l_dev, 0, 8);
	memset(l_virtual_ip, 0, 24);
	memset(l_mac, 0, 24);
	memset(tag, 0, 8);

	if ( LAN == type ) {
		strncpy(l_dev, g_lan_dev, 8);
		strncpy(l_virtual_ip, g_virtual_lan_ip, 24);
		strncpy(l_mac, g_lan_mac, 24);
		strcpy(tag, "LAN");
	} else if ( WAN == type ) {
		strncpy(l_dev, g_wan_dev, 8);
		strncpy(l_virtual_ip, g_virtual_wan_ip, 24);
		strncpy(l_mac, g_wan_mac, 24);
		strcpy(tag, "WAN");
	} else if ( WAN1 == type ) {
		strncpy(l_dev, g_wan1_dev, 8);
		strncpy(l_virtual_ip, g_virtual_wan1_ip, 24);
		strncpy(l_mac, g_wan1_mac, 24);
		strcpy(tag, "WAN1");
	} else {
		return;
	}

	printf("type[%s] vstatus[%d] msg[%s] opt[%d]\n", tag, vstatus, msg, opt);

	if ( OPT_ADD == opt ) {
		/* Add if inactive */
		if ( INACTIVE_STATUS == vstatus ) {
			if( 0 == ip_add(l_dev, l_virtual_ip)) {
				if ( WAN == type ) {
					route_add("default", g_virtual_wan_gateway);
				}
				ip_mac_broadcast(l_dev, l_virtual_ip, l_mac);

				bzero(logbuf, LineMaxLen);
				sprintf(logbuf, "%s - Virtual %s %s [Inactive->Active]!", msg, tag, l_virtual_ip);
				do_log(logbuf);
			}
		} else {
			ip_mac_broadcast(l_dev, l_virtual_ip, l_mac);
		}
	} else if ( OPT_REMOVE == opt ) {
		/* Remove if active */
		if ( ACTIVE_STATUS == vstatus ) {
			if( 0 == ip_del(l_dev, l_virtual_ip)) {
				clear_arp_proxy();
				bzero(logbuf, LineMaxLen);
				sprintf(logbuf, "%s - Virtual %s %s [Active->Inactive]!", msg, tag, l_virtual_ip);
				do_log(logbuf);
			}
		}
	}

	return;
}

void virtual_ip_add(int type, int vstatus, char *msg)
{
	virtual_ip_action(type, vstatus, msg, OPT_ADD);
}

void virtual_ip_remove(int type, int vstatus, char *msg)
{
	virtual_ip_action(type, vstatus, msg, OPT_REMOVE);
}

extern int vpn_need;

void virtual_vpn_start(int vpn_status, char *msg)
{
	char logbuf[LineMaxLen];

	if ( vpn_need && ACTIVE_STATUS != vpn_status ) {
		if ( 0 == client_reset(g_type) ) {
			do_vpn_restart();
			bzero(logbuf, LineMaxLen);
			sprintf(logbuf, "%s - VPN reset and restart!", msg);
			do_log(logbuf);
		}
	}
}

void virtual_vpn_stop(int vpn_status, char *msg)
{
	char logbuf[LineMaxLen];

	if ( vpn_need && (ACTIVE_STATUS==vpn_status || INDEX_DIGITAL_FAILED_STATUS==vpn_status) ) {
		do_vpn_stop();
		bzero(logbuf, LineMaxLen);
		sprintf(logbuf, "%s - VPN stop!", msg);
		do_log(logbuf);
	}
}

void vpn_action(int vpn_status, char *msg, short opt)
{
	if ( OPT_ADD == opt ) {
		virtual_vpn_start(vpn_status, msg);
	} else if ( OPT_REMOVE == opt ) {
		virtual_vpn_stop(vpn_status, msg);
		// inernet vpn stop, add backup vpn routes -> VPN device
		ip_add_static_route();
	}
}

void print_traffic(const char *buffer, int len)
{
	int i=0;

	return;

	for ( i=0; i<len; i++ ) {
		printf("\tbuffer[%d]=-%d-%c-\n", i, (int)buffer[i], buffer[i]);
	}
}
