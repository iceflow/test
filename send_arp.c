#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/ioctl.h>
//#include <linux/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/if_ether.h>


#if __GLIBC__ >= 2 && __GLIBC_MINOR >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>
#else
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#endif

/* local definitions */
#define IF_NAMESIZ    20    /* Max interface lenght size */
#define ETHERNET_HW_LEN     6
#define IPPROTO_ADDR_LEN    4

typedef struct {
	unsigned char src_hw_addr[6];
	struct in_addr sin_addr;
	struct in_addr sin_brd;
    char ifname[IF_NAMESIZ + 1];    /* Interface name */
    unsigned int ifindex;       /* Interface index */	

} m_arp_info;

m_arp_info arp_info;

int get_ifaceindex(int fd, const char *name) {
    struct ifreq ifr;
    if(name == NULL)
		return -1;

    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, name);
	if(ioctl(fd, SIOCGIFINDEX, &ifr) == -1) {
        printf("ioctl error!\n");
        return -1;
    }

	return ifr.ifr_ifindex;
}

int mac_packet(char *buffer, int len, unsigned char *src_mac, __be16 h_proto) {
    memset(buffer, 0, len);
    struct ethhdr *eth = (struct ethhdr *)buffer;

    memset(eth->h_dest, 0xFF, ETH_ALEN);
    memcpy(eth->h_source, src_mac, ETH_ALEN);
    eth->h_proto = h_proto;
    return 0;
}

int arp_packet(char *buffer, unsigned int  src_ip, unsigned int dst_ip) {
	struct ethhdr *eth = (struct ethhdr *)buffer;
	struct ether_arp *arphdr = (struct ether_arp*)(buffer + ETH_HLEN);

	//printf("arp_packet src_ip[%0X]\n", src_ip);

	arphdr->ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
	arphdr->ea_hdr.ar_pro =  htons(ETHERTYPE_IP);
	arphdr->ea_hdr.ar_hln = ETHERNET_HW_LEN;
	arphdr->ea_hdr.ar_pln = IPPROTO_ADDR_LEN;
	//arphdr->ea_hdr.ar_op = htons(0x0001);
	arphdr->ea_hdr.ar_op = htons(ARPOP_REPLY);

	memcpy(arphdr->arp_sha, eth->h_source, 6);
	memcpy(arphdr->arp_tha, eth->h_dest, 6);

	memcpy(arphdr->arp_spa, &src_ip, 4);
	memcpy(arphdr->arp_tpa, &dst_ip, 4);

	return (ETH_HLEN + sizeof(struct ether_arp));
}


//void arp_send(int fd, char *src_ip, char *src_mac, char *dst_ip, char *dst_mac, const char *dev) {
void arp_send(int fd, m_arp_info *info) {
	struct sockaddr_ll sll;	
	char buff[1024] = {0};
	char *buffer = buff;
	int len = 0;
	int ret;

	memset(&sll, 0, sizeof(sll));

	unsigned int src_addr = 0;
	unsigned int dst_addr = 0;

	sll.sll_family = AF_PACKET;
	sll.sll_ifindex = info->ifindex;

#if 0
	if(bind(fd, (struct sockaddr *)&sll, sizeof(sll)) == -1) {
		fprintf(stderr, "bind error:%s!\n", strerror(errno));
		close(fd);
		fd = -1;
		return ;
	}	
#endif

	len = mac_packet(buffer, 1024, info->src_hw_addr, htons(ETHERTYPE_ARP));
	src_addr = info->sin_addr.s_addr;
	dst_addr = info->sin_addr.s_addr;

	len = arp_packet(buffer,  src_addr, dst_addr);	

	ret = sendto(fd, buff, 60, 0, (struct sockaddr *)&sll, sizeof(struct sockaddr_ll));
}


void usage(char *name) {
	fprintf(stderr, "%s  src_ip_addr src_hw_addr targ_ip_addr tar_hw_addr dev_name\n", name);
}

void parse_argvs(m_arp_info *info, int fd, char *ip, char *mac, char *dev)
{
	int i = 0;
	int hw[6];

	memset(info, 0, sizeof(m_arp_info));

	info->sin_addr.s_addr = inet_addr(ip);

	sscanf(mac, "%X:%X:%X:%X:%X:%X", &hw[0], &hw[1], &hw[2], &hw[3], &hw[4], &hw[5]);

	for (i=0; i<6; i++ ){
		info->src_hw_addr[i] = (unsigned char)hw[i];
	}

	memcpy(info->ifname, dev, IF_NAMESIZ);
	info->ifindex = get_ifaceindex(fd, dev);
	return;
}

int main(int argc, char *argv[]) {
	int sockfd = -1;
	if(argc != 4) {
		usage(argv[0]);
		exit(0);
	}

		
	//sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_RARP));
	if(sockfd < 0) {
		fprintf(stderr, "socket error %s\n", strerror(errno));
		return -1;
	}

	parse_argvs(&arp_info, sockfd, argv[1], argv[2], argv[3]);

	arp_send(sockfd, &arp_info);  

	close(sockfd);

	return(0);
}
