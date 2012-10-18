#include <stdio.h> 
#include <string.h>
#include <unistd.h>
#include <stdlib.h> 
#define _GNU_SOURCE
#include <getopt.h>
#include <syslog.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
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
int sockfd=0;

void daemon_main()
{
    struct sockaddr_in servaddr;
    char buf[LineMaxLen];
    int n_byte;
    time_t	now;
    long turns;

    char logbuf[LineMaxLen];

    HB_PACKET peer_hb,peer_last_hb;
    //HB_PACKET peer_hb,peer_last_hb,l_hb;
    int down_time=0; /* Recv peer_hb down times */
    //short first_time=1;
    const int on = 1;


    // 1. socket 
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if ( sockfd <= 0 ) {
        bzero(logbuf, LineMaxLen);
        sprintf(logbuf, "daemon_main: getting socket() error.!");
        do_log(logbuf);
        return;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(HB_PORT);

    if ( 0 != setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) ) {
        bzero(logbuf, LineMaxLen);
	sprintf(logbuf, "daemon_main: setsockopt() error.!");
	do_log(logbuf);
	return;
    }
        
    if ( 0 != bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) ) {
        bzero(logbuf, LineMaxLen);
	sprintf(logbuf, "daemon_main: bind() error[%s].!", strerror(errno));
	do_log(logbuf);

	return;
    }

    turns=0;
    while ( 1 ) {
        if ( debug ) {
            now=time(NULL);
            printf("\n\n====================== Recv Circle (Turns %ld): %s", ++turns, ctime(&now));
        }

        // 1. Recv packet
        bzero(&peer_hb, sizeof(HB_PACKET));
        if ( 0 == readable_timeo(sockfd, g_down_time) ) {
			if ( debug ) printf("[%d]: socket recv timeout(%ds)\n", g_type, g_down_time);
			peer_hb.wan_status=TIMEOUT_STATUS;
        } else {
            bzero(buf, LineMaxLen);
            n_byte=recv(sockfd, buf, LineMaxLen, 0);
            if ( -1 == n_byte ) {
                if (debug) printf("[%d]: can not recv feedback error:[%s]\n", g_type, strerror(errno));
                continue;
            } else { // recv ok
                if ( n_byte < sizeof(HB_PACKET) ) {
                    if (debug) printf("[%d]: Invalid recv format\n", g_type);
                    continue;
                }

                if ( debug ) {
                    printf("[%d]: Recv buffer\n", g_type);
                    print_traffic(buf, n_byte);
				}

                // Check Validation
                memset(&peer_hb, 0, sizeof(HB_PACKET));
                memcpy(&peer_hb, buf, sizeof(HB_PACKET));
                if ( MSG_SEND_STATUS == peer_hb.msg_type ) {

                } else {
                    if (debug) {
                        printf("Invalid msg_type [%d] , packet dropped!\n", peer_hb.msg_type);
                        syslog(LOG_DEBUG, "Invalid msg_type [%d] , packet dropped!\n", peer_hb.msg_type);
					}
                    continue;
                }

                if ( peer_hb.cluster_id == g_type ) {
                    if (debug) {
                        printf("[%d]: Received self packet . Ignore it!\n", peer_hb.cluster_id);
                        syslog(LOG_INFO, "[%d]: Received self packet . Ignore it!\n", peer_hb.cluster_id);
                    }
                    continue;
                }

                if ( peer_hb.cluster_mode != g_mode ) {
                    if (debug) {
                        printf("[%d]: Invalid cluster_mode ,(recv cluster_mode[%d]!=local cluster_mode[%d], packet dropped!\n", peer_hb.msg_type, peer_hb.cluster_mode, g_mode);
                        syslog(LOG_DEBUG, "[%d]: Invalid cluster_mode ,(recv cluster_mode[%d]!=local cluster_mode[%d], packet dropped!\n", peer_hb.msg_type, peer_hb.cluster_mode, g_mode);
                    }
                    continue;
	        }

                // Parse buf
                if ( debug ) {
                    printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
                    printf("Recv HB packet :\n");
                    printf("     msg_type          : %d\n", peer_hb.msg_type);
                    printf("     cluster_id        : %d\n", peer_hb.cluster_id);
                    printf("     cluster_mode      : %d\n", peer_hb.cluster_mode);
                    printf("     cluster_type      : %d \n", peer_hb.cluster_type);
                    printf("             CLUSTER_TYPE_NONE      0\n");
                    printf("             CLUSTER_TYPE_VPN       1\n");
                    printf("             CLUSTER_TYPE_INTERNET  2\n");
                    printf("             CLUSTER_TYPE_MASTER    3\n");
                    printf("             CLUSTER_TYPE_BACKUP    4\n");
                    printf("     wan_status         : ");
                    switch ( peer_hb.wan_status ) {
                        case UP_STATUS:		printf("UP_STATUS\n"); break;
                        case DOWN_STATUS:	printf("DOWN_STATUS\n"); break;
                        case TIMEOUT_STATUS:	printf("TIMEOUT_STATUS\n"); break;
                        default:	printf("%d [UNKNOWN]\n", peer_hb.wan_status);	break;
                    }
                    printf("     wan1_status        : ");
                    switch ( peer_hb.wan1_status ) {
                        case UP_STATUS:		printf("UP_STATUS\n"); break;
                        case DOWN_STATUS:	printf("DOWN_STATUS\n"); break;
                        case TIMEOUT_STATUS:	printf("TIMEOUT_STATUS\n"); break;
                        default:	printf("%d [UNKNOWN]\n", peer_hb.wan_status);	break;
                    }
                    printf("     virtual_wan_status : ");
                    switch ( peer_hb.virtual_wan_status ) {
                        case INACTIVE_STATUS:	printf("INACTIVE_STATUS\n"); break;
                        case ACTIVE_STATUS:	printf("ACTIVE_STATUS\n"); break;
                        default:	printf("%d [UNKNOWN]\n", peer_hb.virtual_wan_status);	break;
                    }
                    printf("     virtual_wan1_status : ");
                    switch ( peer_hb.virtual_wan1_status ) {
                        case INACTIVE_STATUS:	printf("INACTIVE_STATUS\n"); break;
                        case ACTIVE_STATUS:	printf("ACTIVE_STATUS\n"); break;
                        default:	printf("%d [UNKNOWN]\n", peer_hb.virtual_wan1_status);	break;
                    }
                    printf("     virtual_lan_status : ");
                    switch ( peer_hb.virtual_lan_status ) {
                        case INACTIVE_STATUS:	printf("INACTIVE_STATUS\n"); break;
                        case ACTIVE_STATUS:	printf("ACTIVE_STATUS\n"); break;
                        default:	printf("%d [UNKNOWN]\n", peer_hb.virtual_lan_status);	break;
                    }
                    printf("     vpn_status         : ");
                    switch ( peer_hb.vpn_status ) {
                        case INACTIVE_STATUS:	printf("INACTIVE_STATUS\n"); break;
                        case ACTIVE_STATUS:	printf("ACTIVE_STATUS\n"); break;
			case INDEX_DIGITAL_FAILED_STATUS: printf("INDEX_DIGITAL_FAILED_STATUS\n");
                        default:	printf("%d [UNKNOWN]\n", peer_hb.vpn_status);	break;
                    }
                    printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
                }				
            } // End - if ( -1 == n_byte )
        } // End -- if ( 0 == readable_timeo(sockfd, g_down_time) )

#if 0
	// 2. local status check
	bzero(&l_hb, sizeof(HB_PACKET));
	if ( 0 != cluster_check(&l_hb) ) {
	    if (debug) {
	        printf("local cluster_check error!\n");
	    }
	    bzero(buf, LineMaxLen);
	    continue;
	}
	if ( debug ) {
	    printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
	    printf("Local cluster_check :\n");
/*
	    printf("     cluster_mode      : %d\n", peer_hb.cluster_mode);
	    printf("     cluster_type      : %d \n", peer_hb.cluster_type);
	    printf("             CLUSTER_TYPE_NONE      0\n");
	    printf("             CLUSTER_TYPE_VPN       1\n");
	    printf("             CLUSTER_TYPE_INTERNET  2\n");
	    printf("             CLUSTER_TYPE_MASTER    3\n");
	    printf("             CLUSTER_TYPE_BACKUP    4\n");
*/
	    printf("     wan_status         : ");
	    switch ( l_hb.wan_status ) {
			case UP_STATUS:		printf("UP_STATUS\n"); break;
			case DOWN_STATUS:	printf("DOWN_STATUS\n"); break;
			case TIMEOUT_STATUS:	printf("TIMEOUT_STATUS\n"); break;
			default:	printf("%d [UNKNOWN]\n", l_hb.wan_status);	break;
	    }
	    printf("     wan1_status        : ");
	    switch ( l_hb.wan1_status ) {
			case UP_STATUS:		printf("UP_STATUS\n"); break;
			case DOWN_STATUS:	printf("DOWN_STATUS\n"); break;
			case TIMEOUT_STATUS:	printf("TIMEOUT_STATUS\n"); break;
			default:	printf("%d [UNKNOWN]\n", l_hb.wan_status);	break;
	    }
	    printf("     virtual_wan_status : ");
	    switch ( l_hb.virtual_wan_status ) {
			case INACTIVE_STATUS:	printf("INACTIVE_STATUS\n"); break;
			case ACTIVE_STATUS:	printf("ACTIVE_STATUS\n"); break;
			default:	printf("%d [UNKNOWN]\n", l_hb.virtual_wan_status);	break;
		}
	    printf("     virtual_lan_status : ");
	    switch ( l_hb.virtual_lan_status ) {
			case INACTIVE_STATUS:	printf("INACTIVE_STATUS\n"); break;
			case ACTIVE_STATUS:	printf("ACTIVE_STATUS\n"); break;
			default:	printf("%d [UNKNOWN]\n", l_hb.virtual_lan_status);	break;
		}
	    printf("     vpn_status         : ");
	    switch ( l_hb.vpn_status ) {
			case INACTIVE_STATUS:	printf("INACTIVE_STATUS\n"); break;
			case ACTIVE_STATUS:	printf("ACTIVE_STATUS\n"); break;
			case INDEX_DIGITAL_FAILED_STATUS:	printf("INDEX_DIGITAL_FAILED_STATUS\n"); break;
			default:	printf("%d [UNKNOWN]\n", l_hb.vpn_status);	break;
	    }
	    printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
	}
#endif

        if ( TIMEOUT_STATUS == peer_hb.wan_status ) {
            if ( CLUSTER_TYPE_VPN == g_type ) {
                peer_hb.cluster_type=CLUSTER_TYPE_INTERNET;
            } else if ( CLUSTER_TYPE_INTERNET == g_type ) {
                peer_hb.cluster_type=CLUSTER_TYPE_VPN;
            } else if ( CLUSTER_TYPE_MASTER == g_type ) {
                peer_hb.cluster_type=CLUSTER_TYPE_BACKUP;
            } else if ( CLUSTER_TYPE_BACKUP == g_type ) {
                peer_hb.cluster_type=CLUSTER_TYPE_MASTER;
            }
			syslog(LOG_DEBUG, "Recv timeout : save_status");
            save_status(&peer_hb, INACTIVE_STATUS);
        } else {
			if ( UP_STATUS == peer_hb.wan_status || UP_STATUS == peer_hb.wan1_status ||
				UP_STATUS == peer_hb.wan2_status || UP_STATUS == peer_hb.wan3_status ) {
				// If UP save immediately
				save_status(&peer_hb, ACTIVE_STATUS);
			} else {
				down_time++;		
				if ( down_time > 2 ) {
					if ( debug ) {
						printf("Down times > 2 : do save_status()\n");
					}
					syslog(LOG_DEBUG, "Down times > 2 : save_status");
					save_status(&peer_hb, ACTIVE_STATUS);

					peer_last_hb=peer_hb;
					down_time = 0;
				}
			}
		}
//////////////////////////////////////////
        //if (first_time) first_time=0;
    } // End - while ( 1 )
			
    return;
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

void shut_down(sig)
{

	if (debug) printf("Reciving shut_down signale[%d]\n", sig);

	if (sockfd>0) close(sockfd);

	exit(0);
}

void sigHandler(int sig)
{
//      char cmd[200];

        switch(sig) {
                case SIGINT:
                case SIGTERM:
                        shut_down(sig);
                        break;
                case SIGALRM:    
			connect_alarm(SIGALRM);	
			break;
                default:         shut_down(sig);        break;
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
                                printf("%s version %s\n", argv[0], CLUSTER_RECV_VERSION);
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

	openlog("CLUSTER_RECV", LOG_NDELAY, LOG_SYSTEM);

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
                do_log("Cluster mode != 2 or 3, needn't start cluster_recv.!");
                do_quit();
        }

        if ( FALSE == foreground ) {
                if ( 0 != become_daemon(CLUSTER_RECV_PIDFILE) ) {
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
