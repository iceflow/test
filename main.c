#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include "local.h"
#include "utils.h"

#define	INTERNAL	10

/////////////////////////////////////////////////////////////////////

int debug;

int main_loop()
{
	FILE *file=NULL;

	char vpn_dev[8];
	char wan_check_site1[24];
	char wan_check_site2[24];
	char wan_check_site3[24];
	char wan1_check_site1[24];
	char wan1_check_site2[24];
	char wan1_check_site3[24];

	char wan_status[8];
	char wan1_status[8];

	int last_wan_status;
	int wan_status;
	int last_wan1_status;
	int wan1_status;

	bzero(vpn_dev, 8);
        bzero(wan_check_site1, 24);
        bzero(wan_check_site2, 24);
        bzero(wan_check_site3, 24);
        bzero(wan1_check_site1, 24);
        bzero(wan1_check_site2, 24);
        bzero(wan1_check_site3, 24);

        if ( NULL == (file=fopen(CLUSTER_CONF_FILE, "r")) ) return(1);

        GetProfile(file, '=', "VPN_DEV", vpn_dev);
        GetProfile(file, '=', "WAN_CHECK_SITE1", wan_check_site1);
        GetProfile(file, '=', "WAN_CHECK_SITE2", wan_check_site2);
        GetProfile(file, '=', "WAN_CHECK_SITE3", wan_check_site3);
        GetProfile(file, '=', "WAN1_CHECK_SITE1", wan1_check_site1);
        GetProfile(file, '=', "WAN1_CHECK_SITE2", wan1_check_site2);
        GetProfile(file, '=', "WAN1_CHECK_SITE3", wan1_check_site3);
        fclose(file);

	while ( 1 ) {
		bzero(wan_status, 8);
		bzero(wan1_status, 8);
		if ( NULL == (file=fopen(POLICYROUTER_RUNNING, "r")) ) {
			sleep(INTERNAL);
			continue;
		}

		GetProfile(file, '=', "WAN_STATUS", wan_status);
		GetProfile(file, '=', "WAN1_STATUS", wan1_status);

		fclose(file);

		// WAN connection check
		if ( 0 == strcmp(wan_status, "UP")) {
			if ( Connection_Check(wan_check_site1)) wan_status=UP;
			else if ( Connection_Check(wan_check_site2)) wan_status=UP;
			else if ( Connection_Check(wan_check_site3)) wan_status=UP;
			else wan_status=DOWN;
		} else {
			wan_status=DOWN;
		}

		// WAN1 connection check
		if ( 0 == strcmp(wan1_status, "UP")) {
			if ( Connection_Check(wan1_check_site1)) wan1_status=UP;
			else if ( Connection_Check(wan1_check_site2)) wan1_status=UP;
			else if ( Connection_Check(wan1_check_site3)) wan1_status=UP;
			else wan1_status=DOWN;
		} else {
			wan1_status=DOWN;
		}

	}
        
	return(0);
}

//// Cluster mode=3 ,, vpn cluster
int main(int argc, char **argv)
{
	become_daemon(CLUSTER_PIDFILE);

	main_loop();

	exit(0);
}
