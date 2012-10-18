#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <libgen.h>

#include "local.h"
#include "utils.h"

/////////////////////////////////////////////////////////////////////

#define	CHECK_INTERVAL	10

int debug=FALSE;
int vpn_need=TRUE;

void clear_tag()
{
	char filename[LineMaxLen];
	// clear old message

        // vpn
        bzero(filename, sizeof(filename));
        sprintf(filename, "%s.vpn", CLUSTER_RUNNING);
        unlink(filename);
        // internet
        bzero(filename, sizeof(filename));
        sprintf(filename, "%s.internet", CLUSTER_RUNNING);
        unlink(filename);
        // master
        bzero(filename, sizeof(filename));
        sprintf(filename, "%s.master", CLUSTER_RUNNING);
        unlink(filename);
        // backup
        bzero(filename, sizeof(filename));
        sprintf(filename, "%s.backup", CLUSTER_RUNNING);
        unlink(filename);

}

/* 0 - ok , 1 - error */
int safe_cluster_stop()
{
	int pid;
	int Ret;
	char cmd[LineMaxLen];

	pid=get_pid_from_file(SAFE_CLUSTER_PIDFILE);
	if ( pid > 0 ) Ret=kill(pid, SIGTERM);
	
	pid=get_pid_from_file(CLUSTER_SEND_PIDFILE);
	if ( pid > 0 ) Ret=kill(pid, SIGTERM);

	pid=get_pid_from_file(CLUSTER_RECV_PIDFILE);
	if ( pid > 0 ) Ret=kill(pid, SIGTERM);

	pid=get_pid_from_file(HTTPD_SYNC_PIDFILE);
	if ( pid > 0 ) Ret=kill(pid, SIGTERM);

	bzero(cmd, LineMaxLen);
	strncpy(cmd, "killall -9 sync_conf.sh > /dev/null 2>&1 ; killall -9 sync_conf.sh > /dev/null 2>&1", LineMaxLen);	

	system(cmd);

	clear_tag();

	return(0);
}

int safe_cluster_start()
{
        char cmd[LineMaxLen];

	if (!is_running(CLUSTER_SEND_PIDFILE, CLUSTER_SEND)) {
		bzero(cmd, LineMaxLen);
		sprintf(cmd, "%s >/dev/null 2>&1", CLUSTER_SEND);
		system(cmd);
	}

	if (!is_running(CLUSTER_RECV_PIDFILE, CLUSTER_RECV)) {
		bzero(cmd, LineMaxLen);
		sprintf(cmd, "%s >/dev/null 2>&1", CLUSTER_RECV);
		system(cmd);
	}

	return(0);
}

static int make_http_sync_conf_file()
{
	FILE *file=NULL;

	if ( NULL == (file=fopen(HTTPD_SYNC_CONF_FILE, "w")) ) {
		return(-1);
	}

	fprintf(file, "server.document-root = \"/var/conf/sync\"\n");
	fprintf(file, "server.pid-file = \"%s\"\n", HTTPD_SYNC_PIDFILE);
	fprintf(file, "server.port = %d\n", SYNC_WEB_PORT);
	fprintf(file, "server.modules              = (\n");
	fprintf(file, "        \"mod_access\",\n");
	fprintf(file, "        \"mod_cgi\")\n");
	fprintf(file, "mimetype.assign = (\n");
	fprintf(file, "\".htm\"          =>      \"text/html\",\n");
	fprintf(file, "\".js\"           =>      \"text/javascript\",\n");
	fprintf(file, "\".css\"          =>      \"text/css\",\n");
	fprintf(file, "\".gif\"          =>      \"image/gif\",\n");
	fprintf(file, "\".jpg\"          =>      \"image/jpeg\",\n");
	fprintf(file, "\".class\"        =>      \"application/octet-stream\",\n");
	fprintf(file, "\".log\"          =>      \"text/plain\",\n");
	fprintf(file, "\".conf\"         =>      \"text/plain\",\n");
	fprintf(file, "\".text\"         =>      \"text/plain\",\n");
	fprintf(file, "\".txt\"          =>      \"text/plain\",\n");
	fprintf(file, ")\n");
	fprintf(file, "index-file.names = ( \"index.htm\", \"index.html\" )\n");
	fprintf(file, "$HTTP[\"url\"] =~ \"^/cgi-bin/\" {\n");
	fprintf(file, "        cgi.assign = ( \"\" => \"\" )\n");
	fprintf(file, "}\n");

	fclose(file);

	return(0);
}

void check_web()
{
	char cmd[LineMaxLen];

	if (!is_running(HTTPD_SYNC_PIDFILE, LIGHTTPD_CMD)) {
		
		if ( 0 != make_http_sync_conf_file() ) {
			return;
		}

		bzero(cmd, LineMaxLen);
		sprintf(cmd, "%s -f %s >/dev/null 2>&1", LIGHTTPD_CMD, HTTPD_SYNC_CONF_FILE);
		system(cmd);
	}
}

void check_sync()
{
	char cmd[LineMaxLen];
	char buf[LineMaxLen];
	FILE *pfile=NULL;

	bzero(cmd, LineMaxLen);
	strncpy(cmd, "ps aux|grep /ice/mod/cluster/sync_conf.sh | grep -cv grep", 100);
	
	if ( NULL == (pfile=popen(cmd, "r")) ) return;

	bzero(buf, LineMaxLen);
	fgets(buf, LineMaxLen, pfile);

	pclose(pfile);

	if ( '0' == buf[0] ) {
		bzero(cmd, LineMaxLen);
		
		sprintf(cmd, "killall -9 sync_conf.sh > /dev/null 2>&1 ; killall -9 sync_conf.sh > /dev/null 2>&1 ; %s -s /ice/mod/cluster/sync_conf.sh > /dev/null 2>&1", STARTPROC);	
		system(cmd);
	}

}

int main_loop()
{
	FILE *file=NULL;
	char mode[8];

	clear_tag();


	while ( 1 ) {
		bzero(mode, 8);
		if ( NULL == (file=fopen(CLUSTER_CONF_FILE, "r")) ) return(1);
		get_profile(file, '=', "MODE", mode);
		fclose(file);

		check_web();

		check_sync();
	
		if ( streq(mode, "2") || streq(mode, "3") ) {
			safe_cluster_start();
		}

		sleep(CHECK_INTERVAL);
	}

	return(0);
}

int main(int argc, char **argv)
{
	if ( 2==argc ) {
		if (0 == strcmp(argv[1], "stop")) {
			if ( 0 == safe_cluster_stop()) exit(0);
			else exit(1);
		} else if (0 == strcmp(argv[1], "-v") || 0 == strcmp(argv[1], "version") ) {
			printf("%s version %s\n", argv[0], SAFE_CLUSTER_VERSION);
			exit(2);
		}
	}

	become_daemon(SAFE_CLUSTER_PIDFILE);

	main_loop();

	exit(0);
}
