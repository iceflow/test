#ifndef	_GLOBAL_H
#define	_GLOBAL_H

extern int debug;

extern short g_mode;
extern short g_type;
extern char  g_line_select[24];
extern	int g_virtual_lan_enable;
extern char  g_virtual_lan_ip[24];
extern	int g_virtual_wan_enable;
extern char  g_virtual_wan_ip[24];
extern char  g_virtual_wan_gateway[24];
extern	int g_virtual_wan1_enable;
extern char  g_virtual_wan1_ip[24];
extern char  g_virtual_wan1_gateway[24];
extern char  g_hb_ip[24];
extern int   g_down_time;

extern char  g_lan_dev[8];
extern char  g_wan_dev[8];
extern char  g_wan1_dev[8];
extern char  g_lan_mac[24];
extern char  g_wan_mac[24];
extern char  g_wan1_mac[24];

#endif
