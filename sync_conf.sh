#!/bin/sh

CLUSTER_CONF_FILE="/var/conf/cluster.conf"
USER_DB="sync_rules.db"
PLUS_NOTICE="/var/tmp/.plus.notice"

INTERVAL=10

# Making need encrypted user conf file
function do_master()
{
	echo "do_master"

	mkdir -p /var/conf/sync
	cd /var/conf

	TAR_FILE=" "

	test -f sync_rules.tgz && rm sync_rules.tgz

	if [ "_${SYNC_PPTP_ENABLE}" = "_yes" ]; then
		md5sum pptpuser.conf > pptpuser.md5
		TAR_FILE="pptpuser.conf pptpuser.md5"
	fi
	if [ "_${SYNC_SSL_ENABLE}" = "_yes" ]; then
		md5sum ssluser.conf > ssluser.md5
		TAR_FILE="${TAR_FILE} ssluser.conf ssluser.md5"
	fi
	if [ "_${SYNC_WEBSSL_ENABLE}" = "_yes" ]; then
		md5sum webssl_user.conf > webssl_user.md5
		TAR_FILE="${TAR_FILE} webssl_user.conf webssl_user.md5"
	fi

	if [ "_${SYNC_FW_FORWARD_ENABLE}" = "_yes" ]; then
		md5sum forward.ipt > forward.md5
		TAR_FILE="${TAR_FILE} forward.ipt forward.md5"
	fi

	tar zcpf /var/conf/sync/sync_rules.tgz ${TAR_FILE}

	cd /var/conf/sync
	test -f $USER_DB && rm $USER_DB

	/ice/bin/encrypt -f sync_rules.tgz -t $USER_DB
}

function check_file ()
{
	CONF_FILE=$1
	MD5_FILE=$2

	test -f $CONF_FILE || return 1
	test -f $MD5_FILE || return 1

	if [ `md5sum -c $MD5_FILE 2>/dev/null | grep -c ": OK"` -eq 0 ]; then
		echo "Download $CONF_FILE $MD5_FILE md5 check error!"
		return 1
	fi

	diff $CONF_FILE /var/conf/$CONF_FILE > /dev/null 2>&1

	if [ $? -ne 0 ]; then
		echo "$CONF_FILE changed"
		cp $CONF_FILE /var/conf/

		if [ "$1" = "pptpuser.conf" ]; then
			PPTP_CHG=1
		elif [ "$1" = "ssluser.conf" ]; then
			SSL_CHG=1
		elif [ "$1" = "webssl_user.conf" ]; then
			WEBSSL_CHG=1
		elif [ "$1" = "forward.ipt" ]; then
			FW_CHG=1
		fi
	fi
}

# Do save conf 
function do_save_conf()
{
	CLI="/ice/console/CLI"

	echo "do_save_conf()"

	mount conf /conf -o remount,rw
	
	cd /var/conf

	if [ $PPTP_CHG -eq 1 ]; then
		echo "making PPTP"
		$CLI call pap_secrets_rebuild
		$CLI call chap_secrets_rebuild

		cp pptpuser.conf pap-secrets chap-secrets /conf
		$LOGGER -t "SYNC_CONF" -p local3.info "PPTP user synchronized"
	fi

	if [ $SSL_CHG -eq 1 ]; then
		echo "making SSL"
		cp ssluser.conf /conf
		$LOGGER -t "SYNC_CONF" -p local3.info "SSL user synchronized"
		echo "PROFILE=yes" >> ${PLUS_NOTICE}
	fi

	if [ $WEBSSL_CHG -eq 1 ]; then
		echo "making WEBSSL"
		cp webssl_user.conf /conf
		$LOGGER -t "SYNC_CONF" -p local3.info "WebSSL user synchronized"
		echo "PROFILE=yes" >> ${PLUS_NOTICE}
	fi

	if [ $FW_CHG -eq 1 ]; then
		echo "Firewall Forward"
		$CLI call Ipt_Init
		/ice/ipt/sbin/ipt-r /var/conf/iptable.conf
		cp forward.ipt iptable.conf /conf
		$LOGGER -t "SYNC_CONF" -p local3.info "Firewall forward rules synchronized"
	fi

	mount conf /conf -o remount,ro
}

# SYNC needed , check each file and rebuild local db
function do_sync_files()
{
    mkdir -p /var/conf/sync
    cd /var/conf/sync

	/ice/bin/encrypt -f $USER_DB -t sync_rules.tgz

	if [ ! -f sync_rules.tgz ]; then
		echo "sync_rules.tgz not exist"
		return
	fi

	tar zxpf sync_rules.tgz

	PPTP_CHG=0
	SSL_CHG=0
	WEBSSL_CHG=0
	FW_CHG=0

	check_file pptpuser.conf pptpuser.md5
	check_file ssluser.conf ssluser.md5
	check_file webssl_user.conf webssl_user.md5
	check_file forward.ipt forward.md5

	if [ ${PPTP_CHG} -eq 1 -o ${SSL_CHG} -eq 1 -o ${WEBSSL_CHG} -eq 1 -o ${FW_CHG} -eq 1 ]; then
		do_save_conf
	fi
}

# Getting need encrypted user conf file from master
function do_backup()
{
	echo "do_backup"
	mkdir -p /var/conf/sync
	cd /var/conf/sync

	if [ "_${HB_IP}" = "_" ]; then
		echo "NULL HB_IP . return"
		return
	fi

	rm -fr *.conf *.md5
	test -f ${USER_DB} && rm ${USER_DB}

	wget -c -t 3 -o /dev/null http://${HB_IP}:19997/${USER_DB}

	if [ ! -f ${USER_DB} ]; then
		echo "Getting ${USER_DB} failed . return"
		return
	fi

	do_sync_files
}

while [ 1 ]; do

	test -f ${CLUSTER_CONF_FILE} && . ${CLUSTER_CONF_FILE}

	if [ $MODE -ne 3 ]; then
		sleep $INTERVAL
		continue
	fi
	
	## Now only cluster mode3 (Master/Backup) support
	if [ "_${SYNC_ENABLE}" != "_yes" ]; then
		echo "Conf sync disabled"
		sleep $INTERVAL
		continue
	fi

	if [ "_$TYPE" != "_master" -a "_$TYPE" != "_backup" ]; then
		echo "Invalid type [$TYPE]. Skip!"
		sleep $INTERVAL
		continue
	fi

	if [ "_${SYNC_PPTP_ENABLE}" != "_yes" -a "_${SYNC_SSL_ENABLE}" != "_yes" -a "_${SYNC_WEBSSL_ENABLE}" != "_yes" -a "_${SYNC_FW_FORWARD_ENABLE}" != "_yes" ]; then
		echo "No user conf need sync. Skip."
		sleep $INTERVAL
		continue
	fi
	
	if  [ "_$TYPE" = "_master" ]; then
		do_master	
	elif  [ "_$TYPE" = "_backup" ]; then
		do_backup	
	fi

	sleep $INTERVAL
done
