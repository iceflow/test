#include <stdio.h> 
#include <string.h>
#include <unistd.h>
#include <stdlib.h> 
#define _GNU_SOURCE
#include <getopt.h>
#include <syslog.h>
//#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <signal.h>

#include "local.h"
#include "utils.h"
#include "cluster_utils.h"

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif

int debug=FALSE;
int vpn_need=TRUE;

static short  first_time=1;
//static short  down_time=0;
static long   turns;

void shut_down(sig)
{
	if (debug) printf("Reciving shut_down signale[%d]\n", sig);

	exit(0);
}

void sigHandler(int sig)
{
	switch(sig) {
		case SIGINT:
		case SIGTERM:
			shut_down(sig);
			break;
		case SIGALRM:
			connect_alarm(SIGALRM);
			break;
		default:
			shut_down(sig);
			break;
	}
}

void init_signal()
{
	int k;
	struct sigaction action;

	sigaction (SIGHUP, NULL, &action);
	action.sa_handler= &sigHandler;
	action.sa_flags = 0;
	//for (k=1;k<NSIG;k++) sigaction(k, &action, NULL);
	for (k=1;k<16;k++) sigaction(k, &action, NULL);

	if(debug) printf("init signal success!!!\n");

	return;
}

/* Master and Backup process */
void do_master_line_process(HB_PACKET *hb)
{
    char logbuf[LineMaxLen];	
	HB_PACKET peer_hb; /* Peer HB info */

	short v_wan = OPT_NONE;
	short v_wan1 = OPT_NONE;
	short v_lan = OPT_NONE;
	short vpn = OPT_NONE;
	char cmd[LineMaxLen];
	
	/* Getting peer info */
	if ( 0 != get_status(&peer_hb, CLUSTER_TYPE_BACKUP) ) {
		return;
	}

/*
	Virtual LAN schedule
	1. WAN -> WAN1 mode : 烽火通讯需要这种模式
	2. MAS -> BAK mode  : uncompleted
*/
	bzero(logbuf, LineMaxLen);
	sprintf(logbuf, "[MASTER] WAN %s WAN1 %s, [BACKUP] WAN %s WAN1 %s", hb->wan_status?"UP":"DOWN", 
			hb->wan1_status?"UP":"DOWN", peer_hb.wan_status?"UP":"DOWN", peer_hb.wan1_status?"UP":"DOWN");

	if ( UP_STATUS == hb->wan_status ) {
		if ( g_virtual_wan_enable ) {
			v_wan = OPT_ADD;
		}
		if ( g_virtual_lan_enable ) {
			v_lan = OPT_ADD;
		}
		vpn = OPT_ADD;

		if ( g_virtual_wan1_enable ) {
			if ( UP_STATUS == hb->wan1_status ) {
				v_wan1 = OPT_ADD;
			} else if ( DOWN_STATUS == hb->wan1_status ) {
				v_wan1 = OPT_REMOVE;
			}
		}
	} else if ( DOWN_STATUS == hb->wan_status ) {
		memset(cmd, 0, sizeof(cmd));
		sprintf(cmd, "%s call do_wan_start > /dev/null 2>&1", CLI);
		system(cmd);

		if ( g_virtual_wan_enable ) {
			v_wan = OPT_REMOVE;
		}

		if ( UP_STATUS == peer_hb.wan_status ) {
			if ( g_virtual_lan_enable ) {
				v_lan = OPT_REMOVE;
			}
			vpn = OPT_REMOVE;
		} else if ( UP_STATUS == hb->wan1_status ) {
			if ( g_virtual_wan1_enable ) {
				v_wan1 = OPT_ADD;
			}
			if ( g_virtual_lan_enable ) {
				v_lan = OPT_ADD;
			}
			vpn = OPT_ADD;
		} else if ( DOWN_STATUS == hb->wan1_status ) {
			memset(cmd, 0, sizeof(cmd));
			sprintf(cmd, "%s call do_wan1_start > /dev/null 2>&1", CLI);
			system(cmd);
			if ( g_virtual_wan1_enable ) {
				v_wan1 = OPT_REMOVE;
			}
			if ( UP_STATUS == peer_hb.wan1_status ) {
				if ( g_virtual_lan_enable ) {
					v_lan = OPT_REMOVE;
				}
				vpn = OPT_REMOVE;
			}
		}
	} /* End of - if ( UP_STATUS == hb->wan_status ) { */

	/* Do operate */
	if (debug) {
		printf("v_lan[%d] v_wan[%d] v_wan1[%d] vpn[%d]\n", v_lan, v_wan, v_wan1, vpn);
	}
	virtual_ip_action(LAN, hb->virtual_lan_status, logbuf, v_lan);
	virtual_ip_action(WAN, hb->virtual_wan_status, logbuf, v_wan);
	virtual_ip_action(WAN1, hb->virtual_wan1_status, logbuf, v_wan1);
	vpn_action(hb->vpn_status, logbuf, vpn);

	return;
}

void do_master_device_process(HB_PACKET *hb)
{
    char logbuf[LineMaxLen];	
	HB_PACKET peer_hb; /* Peer HB info */

	short v_wan = OPT_NONE;
	short v_wan1 = OPT_NONE;
	short v_lan = OPT_NONE;
	short vpn = OPT_NONE;
	char cmd[LineMaxLen];

	/* Getting peer info */
	if ( 0 != get_status(&peer_hb, CLUSTER_TYPE_BACKUP) ) {
		return;
	}

/*
	1. MAS -> BAK mode  : 主备设备优先模式 WAN 单IP
*/
	bzero(logbuf, LineMaxLen);
	sprintf(logbuf, "[MASTER] WAN %s WAN1 %s, [BACKUP] WAN %s WAN1 %s", hb->wan_status?"UP":"DOWN", 
			hb->wan1_status?"UP":"DOWN", peer_hb.wan_status?"UP":"DOWN", peer_hb.wan1_status?"UP":"DOWN");

	/* bring up everything */
	if ( UP_STATUS == hb->wan_status ) {
		if ( DOWN_STATUS == hb->virtual_wan_status ) {
			if ( g_virtual_wan_enable ) {
				v_wan = OPT_ADD;
			}
		}	
	} else {
/*
		if ( UP_STATUS == hb->virtual_wan_status ) {
			v_wan = OPT_REMOVE;
		}	
*/
		
		if ( g_virtual_wan_enable ) {
			memset(cmd, 0, sizeof(cmd));

			sprintf(cmd, "%s call do_wan_start > /dev/null 2>&1", CLI);
			system(cmd);
			syslog(LOG_DEBUG, "%s -> Bring up WAN.", logbuf);
		}
	}

	if ( UP_STATUS == hb->wan1_status ) {
		if ( DOWN_STATUS == hb->virtual_wan1_status ) {
			if ( g_virtual_wan1_enable ) {
				v_wan1 = OPT_ADD;
			}
		}	
	} else {
/*
		if ( UP_STATUS == hb->virtual_wan1_status ) {
			v_wan1 = OPT_REMOVE;
		}	
*/

		if ( g_virtual_wan1_enable ) {
			memset(cmd, 0, sizeof(cmd));

			sprintf(cmd, "%s call do_wan1_start > /dev/null 2>&1", CLI);
			system(cmd);
			syslog(LOG_DEBUG, "%s -> Bring up WAN1.", logbuf);
		}
	}

	if ( UP_STATUS == hb->wan_status || UP_STATUS == hb->wan1_status ) {
		if ( DOWN_STATUS == hb->virtual_lan_status ) {
			if ( g_virtual_lan_enable ) {
				v_lan = OPT_ADD;
			}
		}	
		if ( INACTIVE_STATUS == hb->vpn_status ) {
			vpn = OPT_ADD;
		}	
	} else {
		if ( UP_STATUS == hb->virtual_lan_status ) {
			if ( g_virtual_lan_enable ) {
				v_lan = OPT_REMOVE;
			}
		}	
		if ( ACTIVE_STATUS == hb->vpn_status ) {
			vpn = OPT_REMOVE;
		}	
	}

	if ( UP_STATUS == hb->virtual_lan_status ) {
		ip_mac_broadcast(g_lan_dev, g_virtual_lan_ip, g_lan_mac);
	}	

	/* Do operate */
	if (debug) {
		printf("v_lan[%d] v_wan[%d] v_wan1[%d] vpn[%d]\n", v_lan, v_wan, v_wan1, vpn);
	}

	virtual_ip_action(LAN, hb->virtual_lan_status, logbuf, v_lan);
	virtual_ip_action(WAN, hb->virtual_wan_status, logbuf, v_wan);
	virtual_ip_action(WAN1, hb->virtual_wan1_status, logbuf, v_wan1);
	vpn_action(hb->vpn_status, logbuf, vpn);

	return;
}

void do_backup_line_process(HB_PACKET *hb)
{
    char logbuf[LineMaxLen];	
	HB_PACKET peer_hb; /* Peer HB info */

	short v_wan = OPT_NONE;
	short v_wan1 = OPT_NONE;
	short v_lan = OPT_NONE;
	short vpn = OPT_NONE;
	char cmd[LineMaxLen];

	/* Getting peer info */
	if ( 0 != get_status(&peer_hb, CLUSTER_TYPE_MASTER) ) {
		return;
	}

/*
	Virtual LAN schedule
	1. line    WAN -> WAN1 mode : 烽火通讯需要这种模式
	2. devices MAS -> BAK mode  : uncompleted
*/
	bzero(logbuf, LineMaxLen);
	sprintf(logbuf, "[BACKUP] WAN %s WAN1 %s, [MASTER] WAN %s WAN1 %s", hb->wan_status?"UP":"DOWN", 
			hb->wan1_status?"UP":"DOWN", peer_hb.wan_status?"UP":"DOWN", peer_hb.wan1_status?"UP":"DOWN");

	if ( streq(g_line_select, "line") ) {
		if ( UP_STATUS == peer_hb.wan_status ) {
			if ( g_virtual_wan_enable ) {
				v_wan = OPT_REMOVE;
			}
			if ( g_virtual_lan_enable ) {
				v_lan = OPT_REMOVE;
			}
			vpn = OPT_REMOVE;

			if ( UP_STATUS == peer_hb.wan1_status ) {
				if ( g_virtual_wan1_enable ) {
					v_wan1 = OPT_REMOVE;
				}
			} else if ( DOWN_STATUS == peer_hb.wan1_status ) {
				if ( UP_STATUS == hb->wan1_status ) {
					if ( g_virtual_wan1_enable ) {
						v_wan1 = OPT_ADD;
					}
				} else if ( DOWN_STATUS == hb->wan1_status ) {
					if ( g_virtual_wan1_enable ) {
						v_wan1 = OPT_REMOVE;
					}
				}
			}
		} else if ( DOWN_STATUS == peer_hb.wan_status ) {
			/* Bring UP local WANs */
			if ( UP_STATUS == hb->wan_status ) {
				if ( g_virtual_wan_enable ) {
					v_wan = OPT_ADD;
				}
				if ( g_virtual_lan_enable ) {
					v_lan = OPT_ADD;
				}
				vpn = OPT_ADD;

				if ( UP_STATUS == peer_hb.wan1_status ) {
					if ( g_virtual_wan1_enable ) {
						v_wan1 = OPT_REMOVE;
					}
				} else if ( DOWN_STATUS == peer_hb.wan1_status ) {
					if ( UP_STATUS == hb->wan1_status ) {
						if ( g_virtual_wan1_enable ) {
							v_wan1 = OPT_ADD;
						}
					} else if ( UP_STATUS == hb->wan1_status ) {
						if ( g_virtual_wan1_enable ) {
							v_wan1 = OPT_REMOVE;
						}
					}	
				}

			} else if ( DOWN_STATUS == hb->wan_status ) {
				memset(cmd, 0, sizeof(cmd));

				sprintf(cmd, "%s call do_wan_start > /dev/null 2>&1", CLI);
				system(cmd);

				if ( g_virtual_wan_enable ) {
					v_wan = OPT_REMOVE;
				}
				if ( UP_STATUS == peer_hb.wan1_status ) {
					if ( g_virtual_lan_enable ) {
						v_lan = OPT_REMOVE;
					}
					vpn = OPT_REMOVE;
				} else if ( DOWN_STATUS == peer_hb.wan1_status ) {
					memset(cmd, 0, sizeof(cmd));

					sprintf(cmd, "%s call do_wan1_start > /dev/null 2>&1", CLI);
					system(cmd);
					if ( UP_STATUS == hb->wan1_status ) {
						if ( g_virtual_wan1_enable ) {
							v_wan1 = OPT_ADD;
						}
						if ( g_virtual_lan_enable ) {
							v_lan = OPT_ADD;
						}
						vpn = OPT_ADD;
					} else if ( UP_STATUS == hb->wan1_status ) {
						if ( g_virtual_wan1_enable ) {
							v_wan1 = OPT_REMOVE;
						}
					}	
				}
			}
		} /* End of - if ( UP_STATUS == peer_hb.wan_status ) { */
	} else if ( streq(g_line_select, "device") ) {

	} /* End of - if ( streq(g_line_select, "line") ) */

	/* Do operate */
	if (debug) {
		printf("v_lan[%d] v_wan[%d] v_wan1[%d] vpn[%d]\n", v_lan, v_wan, v_wan1, vpn);
	}
	virtual_ip_action(LAN, hb->virtual_lan_status, logbuf, v_lan);
	virtual_ip_action(WAN, hb->virtual_wan_status, logbuf, v_wan);
	virtual_ip_action(WAN1, hb->virtual_wan1_status, logbuf, v_wan1);
	vpn_action(hb->vpn_status, logbuf, vpn);

	return;
}

void do_backup_device_process(HB_PACKET *hb)
{
    char logbuf[LineMaxLen];	
	HB_PACKET peer_hb; /* Peer HB info */

	short v_wan = OPT_NONE;
	short v_wan1 = OPT_NONE;
	short v_lan = OPT_NONE;
	short vpn = OPT_NONE;
	//char cmd[LineMaxLen];

	/* Getting peer info */
	if ( 0 != get_status(&peer_hb, CLUSTER_TYPE_MASTER) ) {
		return;
	}

/*
	1. devices MAS -> BAK mode  : 标准主备模式
*/
	bzero(logbuf, LineMaxLen);
	sprintf(logbuf, "[BACKUP] WAN %s WAN1 %s, [MASTER] WAN %s WAN1 %s", hb->wan_status?"UP":"DOWN", 
			hb->wan1_status?"UP":"DOWN", peer_hb.wan_status?"UP":"DOWN", peer_hb.wan1_status?"UP":"DOWN");

	if ( UP_STATUS == peer_hb.wan_status || UP_STATUS == peer_hb.wan1_status  ) {
		if ( g_virtual_wan_enable ) {
			v_wan = OPT_REMOVE;
		}
		if ( g_virtual_wan1_enable ) {
			v_wan1 = OPT_REMOVE;
		}
		if ( g_virtual_lan_enable ) {
			v_lan = OPT_REMOVE;
		}
		vpn = OPT_REMOVE;
	} else {
		if ( g_virtual_wan_enable ) {
			v_wan = OPT_ADD;
		}
		if ( g_virtual_wan1_enable ) {
			v_wan1 = OPT_ADD;
		}
		if ( g_virtual_lan_enable ) {
			v_lan = OPT_ADD;
		}
		vpn = OPT_ADD;
	}

	if ( UP_STATUS == hb->virtual_lan_status ) {
		ip_mac_broadcast(g_lan_dev, g_virtual_lan_ip, g_lan_mac);
	}	

	/* Do operate */
	if (debug) {
		printf("v_lan[%d] v_wan[%d] v_wan1[%d] vpn[%d]\n", v_lan, v_wan, v_wan1, vpn);
	}
	virtual_ip_action(LAN, hb->virtual_lan_status, logbuf, v_lan);
	virtual_ip_action(WAN, hb->virtual_wan_status, logbuf, v_wan);
	virtual_ip_action(WAN1, hb->virtual_wan1_status, logbuf, v_wan1);
	vpn_action(hb->vpn_status, logbuf, vpn);

	return;
}

//////////////////////////////////////////////

void do_master_process(HB_PACKET *hb)
{
	/* line priority mode */
	if ( streq(g_line_select, "line") ) {
		do_master_line_process(hb);
	/* device priority mode */
	} else {
		do_master_device_process(hb);
	}
}

void do_backup_process(HB_PACKET *hb)
{
	/* line priority mode */
	if ( streq(g_line_select, "line") ) {
		do_backup_line_process(hb);
	/* device priority mode */
	} else {
		do_backup_device_process(hb);
	}
}

void do_vpn_process(HB_PACKET *hb)
{
    char logbuf[LineMaxLen];	
	HB_PACKET peer_hb; /* Peer HB info */

	short v_lan = OPT_NONE;
	short vpn = OPT_NONE;
	
	/* Getting peer info */
	if ( 0 != get_status(&peer_hb, CLUSTER_TYPE_INTERNET) ) {
		return;
	}

/*
	1. VPN -> INT mode : 只支持设备优先, 只有当 INT所有4条WAN都不通了，才接管Virtual LAN
*/
	bzero(logbuf, LineMaxLen);
	sprintf(logbuf, "[VPN] WAN %s WAN1 %s, [INT] WAN %s WAN1 %s", hb->wan_status?"UP":"DOWN", 
			hb->wan1_status?"UP":"DOWN", peer_hb.wan_status?"UP":"DOWN", peer_hb.wan1_status?"UP":"DOWN");

/*
	2. BAK (WAN, WAN1 down), VPN take over LAN Virtual IP
*/
	if ( DOWN_STATUS == peer_hb.wan_status && DOWN_STATUS == peer_hb.wan1_status
		&& DOWN_STATUS == peer_hb.wan2_status && DOWN_STATUS == peer_hb.wan3_status) {
		if ( !hb->virtual_lan_status ) {
			v_lan = OPT_ADD;
		}
	} else {
		if ( hb->virtual_lan_status ) {
			v_lan = OPT_REMOVE;
		}
	}

/*
	3. VPN Check
*/
	if ( UP_STATUS == hb->wan_status || UP_STATUS == hb->wan1_status 
		|| UP_STATUS == hb->wan2_status || UP_STATUS == hb->wan3_status ) {
		if ( INACTIVE_STATUS == hb->vpn_status ) {
			vpn = OPT_ADD;
		}	
	} else {
		if ( ACTIVE_STATUS == hb->vpn_status ) {
			vpn = OPT_REMOVE;
		}	
	}

/*
	4. Broadcast lan virtual IP if needed
*/
	if ( UP_STATUS == hb->virtual_lan_status ) {
		ip_mac_broadcast(g_lan_dev, g_virtual_lan_ip, g_lan_mac);
	}	

	/* Do operate */
	if (debug) {
		printf("v_lan[%d] vpn[%d]\n", v_lan, vpn);
	}
	virtual_ip_action(LAN, hb->virtual_lan_status, logbuf, v_lan);
	vpn_action(hb->vpn_status, logbuf, vpn);

	return;
}

void do_int_process(HB_PACKET *hb)
{
    char logbuf[LineMaxLen];	
	HB_PACKET peer_hb; /* Peer HB info */

	short v_lan = OPT_NONE;
	short vpn = OPT_NONE;
	
	/* Getting peer info */
	if ( 0 != get_status(&peer_hb, CLUSTER_TYPE_VPN) ) {
		return;
	}

/*
	1. VPN -> INT mode : 只支持设备优先, 只有当 INT所有4条WAN都不通了，才接管Virtual LAN
*/
	bzero(logbuf, LineMaxLen);
	sprintf(logbuf, "[INT] WAN %s WAN1 %s, [VPN] WAN %s WAN1 %s", hb->wan_status?"UP":"DOWN", 
			hb->wan1_status?"UP":"DOWN", peer_hb.wan_status?"UP":"DOWN", peer_hb.wan1_status?"UP":"DOWN");

/*
	2. VPN (WAN, WAN1 down), BAK take over vpn
*/
	if ( DOWN_STATUS == peer_hb.wan_status && DOWN_STATUS == peer_hb.wan1_status
		&& DOWN_STATUS == peer_hb.wan2_status && DOWN_STATUS == peer_hb.wan3_status) {
        if ( INACTIVE_STATUS == hb->vpn_status ) {
            vpn = OPT_ADD;
        }
	} else {
        if ( ACTIVE_STATUS == hb->vpn_status ) {
            vpn = OPT_REMOVE;
        }
	}

/*
	3. Virtual LAN check
*/
	if ( UP_STATUS == hb->wan_status || UP_STATUS == hb->wan1_status 
		|| UP_STATUS == hb->wan2_status || UP_STATUS == hb->wan3_status ) {
		if ( !hb->virtual_lan_status ) {
			v_lan = OPT_ADD;
		}
	} else {
		if ( hb->virtual_lan_status ) {
			v_lan = OPT_REMOVE;
		}
	}

/*
	4. Broadcast lan virtual IP if needed
*/
	if ( UP_STATUS == hb->virtual_lan_status ) {
		ip_mac_broadcast(g_lan_dev, g_virtual_lan_ip, g_lan_mac);
	}	

	/* Do operate */
	if (debug) {
		printf("v_lan[%d] vpn[%d]\n", v_lan, vpn);
	}
	virtual_ip_action(LAN, hb->virtual_lan_status, logbuf, v_lan);
	vpn_action(hb->vpn_status, logbuf, vpn);

	return;
}

void daemon_main()
{
    static int sockfd=0;
    static struct sockaddr_in servaddr;
    static const int on=1;

    //time_t now;
    char logbuf[LineMaxLen];
    
    // senbuf
    HB_PACKET m_hb,m_last_hb;
    
    char sendbuf[LineMaxLen];
    
    printf("sizeof HB_PACKET:%d\n", sizeof(HB_PACKET));
    
    // 1. Init socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    if ( sockfd <= 0 ) {
		bzero(logbuf, LineMaxLen);
		sprintf(logbuf, "daemon_main: getting socket() error.!");
		do_log(logbuf);
		return;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(HB_PORT);
    if ( 0 >= inet_pton(AF_INET, g_hb_ip, &servaddr.sin_addr) ) {
		bzero(logbuf, LineMaxLen);
		sprintf(logbuf, "daemon_main: inet_pton() error.!");
		do_log(logbuf);
		return;
    }

    // 2. set option allow boradcast
    if ( -1 == setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &on, sizeof (on)) ) {
		bzero(logbuf, LineMaxLen);
		sprintf(logbuf, "daemon_main: setsockopt() error.!");
		do_log(logbuf);
		return;
    }

    bzero(&m_hb, sizeof(HB_PACKET));
    bzero(&m_last_hb, sizeof(HB_PACKET));

    turns=0;
    while ( 1 ) {
		turns++;

	//dbglog("== [Turn %d] %t: 1 - do_vpn_stop", turns);
        if ( first_time ) do_vpn_stop();

        bzero(&m_hb, sizeof(HB_PACKET));
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

	uint8 reserver[8]; 	// reserver[16]
} HB_PACKET;
*/
        //if ( debug ) {
//	    now=time(NULL);
	    //printf("\n\n====================== Send Circle : %s", ctime(&now));
	    //syslog(LOG_DEBUG, "====================== Send Circle %ld: %s", ++turns, ctime(&now));
        //}

	//dbglog("== [Turn %d] %t: 2 - do_vpn_stop", turns);
	// 1). cluster_check
		if ( 0 != cluster_check(&m_hb) ) {
			if (debug) {
				printf("cluster_check error!\n");
			}
			sleep(INTERVAL);
			continue;
		}

		if ( debug ) {
			printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
			printf("Local cluster_check :\n");
			printf("     wan_status         : ");
			switch ( m_hb.wan_status ) {
				case UP_STATUS:		printf("UP_STATUS\n"); break;
				case DOWN_STATUS:	printf("DOWN_STATUS\n"); break;
				case TIMEOUT_STATUS:	printf("TIMEOUT_STATUS\n"); break;
				default:	printf("%d [UNKNOWN]\n", m_hb.wan_status);	break;
			}
			printf("     wan1_status        : ");
			switch ( m_hb.wan1_status ) {
				case UP_STATUS:		printf("UP_STATUS\n"); break;
				case DOWN_STATUS:	printf("DOWN_STATUS\n"); break;
				case TIMEOUT_STATUS:	printf("TIMEOUT_STATUS\n"); break;
				default:	printf("%d [UNKNOWN]\n", m_hb.wan_status);	break;
			}
			printf("     virtual_wan_status : ");
			switch ( m_hb.virtual_wan_status ) {
				case INACTIVE_STATUS:	printf("INACTIVE_STATUS\n"); break;
				case ACTIVE_STATUS:	printf("ACTIVE_STATUS\n"); break;
				default:	printf("%d [UNKNOWN]\n", m_hb.virtual_wan_status);	break;
			}
			printf("     virtual_wan1_status : ");
			switch ( m_hb.virtual_wan1_status ) {
				case INACTIVE_STATUS:	printf("INACTIVE_STATUS\n"); break;
				case ACTIVE_STATUS:	printf("ACTIVE_STATUS\n"); break;
				default:	printf("%d [UNKNOWN]\n", m_hb.virtual_wan1_status);	break;
			}
			printf("     virtual_lan_status : ");
			switch ( m_hb.virtual_lan_status ) {
				case INACTIVE_STATUS:	printf("INACTIVE_STATUS\n"); break;
				case ACTIVE_STATUS:	printf("ACTIVE_STATUS\n"); break;
				default:	printf("%d [UNKNOWN]\n", m_hb.virtual_lan_status);	break;
			}
			printf("     vpn_status         : ");
			switch ( m_hb.vpn_status ) {
				case INACTIVE_STATUS:	printf("INACTIVE_STATUS\n"); break;
				case ACTIVE_STATUS:	printf("ACTIVE_STATUS\n"); break;
				case INDEX_DIGITAL_FAILED_STATUS:	printf("INDEX_DIGITAL_FAILED_STATUS\n"); break;
				default:	printf("%d [UNKNOWN]\n", m_hb.vpn_status);	break;
			}
			printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
		}

		// 2). make up send packet
		m_hb.msg_type=MSG_SEND_STATUS;
		m_hb.cluster_id=g_type;
		m_hb.cluster_mode=g_mode;
		m_hb.cluster_type=g_type;

		bzero(sendbuf, LineMaxLen);
		memcpy(sendbuf, &m_hb, sizeof(HB_PACKET));

		// 4. Send buffer
		//dbglog("== [Turn %d] %t: 3 - before sendto()", turns);
		sendto(sockfd, sendbuf, sizeof(HB_PACKET), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
		if (debug) {
			printf("Send to : %s -- buf :\n", g_hb_ip);
			print_traffic(sendbuf, sizeof(HB_PACKET));
		}
		//dbglog("== [Turn %d] %t: 4 - after sendto()", turns);

		////////// Do process /////////////

		if ( CLUSTER_MODE_VPN_INT == g_mode ) {
			if ( CLUSTER_TYPE_VPN == g_type ) {
				do_vpn_process(&m_hb);
			} else if ( CLUSTER_TYPE_INTERNET == g_type ) {
				do_int_process(&m_hb);
			}
		} else if ( CLUSTER_MODE_MAS_BAK == g_mode ) {
			if ( CLUSTER_TYPE_MASTER == g_type ) {
				do_master_process(&m_hb);
			} else if ( CLUSTER_TYPE_BACKUP == g_type ) {
				do_backup_process(&m_hb);
			}
		}

		//dbglog("== [Turn %d] %t: 5 - before save_status()", turns);
		save_status(&m_hb, ACTIVE_STATUS);		

		m_last_hb=m_hb;
	////////////////////////////////////////////////////////////////////////////////////////

		if (first_time) first_time=0;

		//dbglog("== [Turn %d] %t: 6 - before sleep()", turns);
		//printf("== [Turn %d] %t: 6 - before sleep(%d)", turns, INTERVAL);
		sleep(INTERVAL);
	}
}

int init_read(char *conf_file)
{
    int ret=0;
    char logbuf[LineMaxLen];	

    ret=cluster_read(conf_file);

    if ( 0 != ret ) {
	bzero(logbuf, LineMaxLen);
	sprintf(logbuf, "cluster_read error. err Ret=[%d]\n", ret);
        do_log(logbuf);	
    }

    return(ret);
}

void showusage(char *prog)
{
    printf("%s:\n", prog);
    printf("Usage: %s [options], where options are:\n\n", prog);
    printf(" [-c] [--conf file]        Specifies the config file to read default\n");
    printf("                           settings from (default is %s).\n", CLUSTER_CONF_FILE);
    printf(" [-e] [--dev file]         Specifies the dev define config file to read default\n");
    printf("                           settings from (default is %s).\n", DEFAULT_DEV_DEFINE);
    printf(" [-n] [--novpn]            No vpn backup needed.\n");
    printf(" [-d] [--debug]            Turns on debugging (to syslog).\n");
    printf(" [-f] [--fg]               Run in foreground.\n");
    printf(" [-h] [--help]             Displays this help message.\n");
    printf(" [-v] [--version]          Displays version.\n");
}

int main(int argc, char **argv)
{
	int foreground = FALSE;
	int c;
	char *configFile=NULL;
	char *devFile=NULL;

	/* process command line options */
	while (1) {
		int option_index = 0;
		char *optstring = "c:e:ndfhv";

		static struct option long_options[] =
		{
			{"conf", 1, 0, 0},
			{"dev", 1, 0, 0},
			{"novpn", 0, 0, 0},
			{"debug", 0, 0, 0},
			{"fg", 0, 0, 0},
			{"help", 0, 0, 0},
			{"version", 0, 0, 0},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, optstring, long_options, &option_index);

		if (c == -1)
			break;
		/* convert long options to short form */
		if (c == 0)
			c = "cendfh"[option_index];

		switch (c) {
			case 'c': /* --conf */
			{
				FILE *f;
				if (!(f = fopen(optarg, "r"))) {
					printf("Config file %s not found!", optarg);
					return 1;
				}
				fclose(f);
				if(configFile) free(configFile);
				configFile = strdup(optarg);
				break;
			}
			case 'e': /* --dev */
			{
				FILE *f;
				if (!(f = fopen(optarg, "r"))) {
					printf("Config file %s not found!", optarg);
					return 1;
				}
				fclose(f);
				if(devFile) free(devFile);
				devFile = strdup(optarg);
				break;
			}
			case 'n': /* --novpn */
				vpn_need = FALSE;
				break;
			case 'd': /* --debug */
				debug = TRUE;
				break;
			case 'f': /* --fg */
				foreground = TRUE;
				break;
			case 'h': /* --help */
				showusage(argv[0]);
				return 0;
			case 'v': /* --version */
				printf("%s version %s\n", argv[0], CLUSTER_SEND_VERSION);
				return 0;
			default:
				showusage(argv[0]);
				return 1;
		}
	}

	/* Parse command ok */
	if (!configFile) configFile = strdup(CLUSTER_CONF_FILE);
	if (!devFile) devFile = strdup(DEFAULT_DEV_DEFINE);

	if (debug) {
		printf("configFile is %s\n", configFile);
		printf("vpn_need is %s\n", (TRUE==vpn_need)?"TRUE":"FALSE");
		printf("foreground is %s\n", (TRUE==foreground)?"TRUE":"FALSE");
	}

	openlog("CLUSTER_SEND", LOG_NDELAY, LOG_SYSTEM);

	do_log("....Start!");

	if ( 0 != get_dev_info(devFile) ) {
		do_log("get_dev_info error!\n");
		do_quit();
	}

	if (debug) {
		printf("devFile is %s\n", devFile);
		printf("lan_dev is %s\n", g_lan_dev);
		printf("wan_dev is %s\n", g_wan_dev);
	}

	if ( 0 != init_read(configFile) ) {
		do_log("init_read error!\n");
		do_quit();
	}

	if (debug) printf("init_read ok!\n");

	if ( CLUSTER_MODE_VPN_INT != g_mode && CLUSTER_MODE_MAS_BAK != g_mode ) {
		do_log("Cluster mode != 2 or 3, needn't start cluster_send.!");
		do_quit();
	}

	if ( FALSE == foreground ) {
		if ( 0 != become_daemon(CLUSTER_SEND_PIDFILE) ) {
			do_log("become_daemon error!\n");
			do_quit();
		}
	}

	if (debug) printf ("Into sub deamon: \n");

	init_signal();

	daemon_main();

	do_quit();

	return 0;
}
