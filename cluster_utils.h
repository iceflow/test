#ifndef	_CLUSTER_UTILS_H
#define	_CLUSTER_UTILS_H

#ifndef	uint8
typedef	unsigned char uint8;
#endif

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

	uint8 reserver[8]; 	// reserver[16]
} HB_PACKET;

extern void connect_alarm(int signo);
extern int wan_check();

extern int get_dev_info(const char *conf_file);
extern int cluster_read(const char *conf_file);
extern void do_quit();
extern void do_log(const char *logbuf);

extern short cluster_check(HB_PACKET *hb);

extern int ip_add(const char *dev, const char *ip);
extern int ip_del(const char *dev, const char *ip);
extern int ip_mac_broadcast(const char *dev, char *ip_addr, char *mac_addr);
//extern int ip_add_static_route();
extern void route_add(const char *dst, const char *nexthop);

extern int ipsec_shotdown();
extern int do_vpn_stop();
extern int do_vpn_start();
extern int do_vpn_restart();
extern int client_reset(int type);

extern int save_status(HB_PACKET *hb, int alive_status);
extern int get_status(HB_PACKET *hb, int cluster_type);
extern void clear_arp_proxy();

extern void virtual_ip_action(int type, int vstatus, char *msg, short opt);
extern void virtual_ip_add(int type, int vstatus, char *msg);
extern void virtual_ip_remove(int type, int vstatus, char *msg);

enum _enum_actions { OPT_NONE, OPT_ADD, OPT_REMOVE };

extern void virtual_vpn_start(int vpn_status, char *msg);
extern void virtual_vpn_stop(int vpn_status, char *msg);
extern void vpn_action(int vpn_status, char *msg, short opt);
extern void print_traffic(const char *buffer, int len);
extern int readable_timeo(int fd, int sec);

#endif
