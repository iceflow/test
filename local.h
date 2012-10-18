#ifndef	_LOCAL_H
#define	_LOCAL_H

#define	SAFE_CLUSTER_VERSION	"1.0"
#define	CLUSTER_SEND_VERSION	"1.0"
#define	CLUSTER_RECV_VERSION	"1.1"

#include "global.h"

// ############  CLUSTER
#define SAFE_CLUSTER            "/ice/mod/cluster/safe_cluster"
#define SAFE_CLUSTER_PIDFILE    "/var/run/safe_cluster.pid"

//#define CLUSTER_PIDFILE		"/var/run/cluster.pid"
//#define	CLUSTER_CMD		"/ice/mod/cluster/cluster"

#define CLUSTER_CONF_FILE       "/var/conf/cluster.conf"
#define PROFILE_FILE            "/var/conf/sys-profile.conf" 
#define BOOT_WAN_FILE           "/var/conf/boot.wan"
#define BOOT_LAN_FILE           "/var/conf/boot.lan"

#define CLUSTER_SEND_PIDFILE	"/var/run/cluster_send.pid"
#define CLUSTER_RECV_PIDFILE	"/var/run/cluster_recv.pid"

#define CLUSTER_SEND		"/ice/mod/cluster/cluster_send"
#define CLUSTER_RECV		"/ice/mod/cluster/cluster_recv"

// sync web
#define	SYNC_WEB_PORT	19997
#define HTTPD_SYNC_CONF_FILE    "/var/conf/httpd_sync.conf"
#define LIGHTTPD_CMD		"/ice/web/sbin/lighttpd"
#define HTTPD_SYNC_PIDFILE	"/var/run/httpd_sync.pid"


// POLICY ROUTING
#define SAFE_POLICY_PIDFILE     "/var/run/safe_policy.pid"
#define POLICYROUTER_PIDFILE    "/var/run/policyrouter.pid"
#define POLICYROUTER_RUNNING    "/var/tmp/.policy.running"

// Cluster running
#define CLUSTER_RUNNING         "/var/tmp/.cluster.running"

#define RUNNING_DEV_DEFINE      "/var/conf/dev.def"
#define	DEFAULT_DEV_DEFINE	RUNNING_DEV_DEFINE

#define CLI "/ice/console/CLI"


// HB port
#define HB_PORT         9997

// CLUSTER_MODE
#define	CLUSTER_MODE_0		0
#define	CLUSTER_MODE_1		1
#define	CLUSTER_MODE_2		2
#define	CLUSTER_MODE_3		3

#define	CLUSTER_MODE_NONE	CLUSTER_MODE_0
#define	CLUSTER_MODE_VPN_BAK	CLUSTER_MODE_1
#define	CLUSTER_MODE_VPN_INT	CLUSTER_MODE_2
#define	CLUSTER_MODE_MAS_BAK	CLUSTER_MODE_3	

// DOWN_TIME
#define	MIN_DOWN_TIME		5

// /////

#define LineMaxLen              1024
#define STARTPROC               "/sbin/startproc"

#define INTERVAL		2

#define	CLUSTER_TYPE_NONE			0
#define	CLUSTER_TYPE_INTERNET			1
#define	CLUSTER_TYPE_VPN			2
#define	CLUSTER_TYPE_MASTER			3
#define	CLUSTER_TYPE_BACKUP			4

// msg type
#define MSG_SEND_STATUS		10

// wan status
#define DOWN_STATUS     0
#define UP_STATUS       1
#define TIMEOUT_STATUS  2

// cluster status
#define	INACTIVE_STATUS			0
#define	ACTIVE_STATUS			1
#define	INDEX_DIGITAL_FAILED_STATUS	2

// Connection check
#define	DOWN			0
#define	UP			1

#define	TRUE		1
#define	FALSE		0

// LOG
#define LOG_MOBILE      LOG_LOCAL2
#define LOG_VPN         LOG_LOCAL3
#define LOG_SYSTEM      LOG_LOCAL4
#define LOG_FIREWALL    LOG_LOCAL5

///////////////////
#define streq(x, y) (!strcmp((x), (y)))
#define strmatch(x, y) (!matches((x), (y)))

// ## 
#define IPSEC_CMD       "/ice/ipsec/sbin/ipsec"
#define	IP_CMD		"/ice/tc/sbin/ip"
#define	SEND_ARP	"/ice/bin/send_arp"

#define	BOOT_ROUTE_FILE	"/var/conf/boot.route"

#define	VIRTUAL_WAN_ADD	"/ice/bin/virtual_wan_add"

// Daemon files
#define VPN_DAEMON_CMD          "/ice/flow/sbin/vpn_daemon"
#define DAEMON_INIT_PID_FILE    "/var/run/daemon-init.pid"
#define DAEMON_PID_FILE         "/var/run/daemon.pid"
#define LICENSE_FILE            "/var/conf/license.conf"
#define LICENSE_FAILED_FILE     "/var/run/license.failed"
#define INDEXDIGITAL_FAILED_FILE     "/var/run/indexdigital.failed"

#define LICENSE_FILE            "/var/conf/license.conf"
#define INDEX_CONF_FILE         "/var/conf/index.conf"

#define POLICYROUTER_RUNNING    "/var/tmp/.policy.running"

#define LICENSE_FAILED_FILE     "/var/run/license.failed"

///
#define Req_Client_Reset        99

#define	DAEMON_PORT		9009

#define ARP                    "/sbin/arp"
#define GREP                   "/bin/grep"
#define AWK                    "/bin/awk"

// Mode
#define LAN             1
#define LAN1            10
#define LAN2            11
#define WAN             2
#define VPN             3
#define WAN1            4
#define WAN2            41
#define WAN3            42
#define FUNC            5
#define VPN1            6
#define VPN2            61
#define VPN3            62
#define PPP             7
#define MODEM           8
#define NOSPEC          9

#endif
