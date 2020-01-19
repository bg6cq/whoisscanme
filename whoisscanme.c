#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/ethernet.h>
//#include <net/if.h>
//#include <net/if_arp.h>
#include <linux/if_packet.h>
#include <linux/if_arp.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>

#define MAX_PACKET_SIZE 2048
#define MAXLEN 2048

#define MY_SEQ 1

#define STOREMYSQL 1

int debug = 0;
char response_str[MAXLEN] = "https://github.com/bg6cq/whoisscanme";

int32_t raw_ifindex;
int32_t raw_sockfd;
char dev_name[MAXLEN];
char dev_mac[6];
int Ports[65536];
FILE *logfile;
int do_response = 1;
uint32_t my_ip;

#ifdef __LITTLE_ENDIAN
#define IPQUAD(addr)			\
    ((unsigned char *)&addr)[0],	\
    ((unsigned char *)&addr)[1],	\
    ((unsigned char *)&addr)[2],	\
    ((unsigned char *)&addr)[3]
#else
#define IPQUAD(addr)			\
  ((unsigned char *)&addr)[3],		\
    ((unsigned char *)&addr)[2],	\
    ((unsigned char *)&addr)[1],	\
    ((unsigned char *)&addr)[0]
#endif

char *stamp(void)
{
	static char st_buf[200];
	struct timeval tv;
	struct timezone tz;
	struct tm *tm;

	gettimeofday(&tv, &tz);
	tm = localtime(&tv.tv_sec);

	snprintf(st_buf, 200, "%02d%02d %02d:%02d:%02d.%06ld", tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec);
	return st_buf;
}

void err_doit(int errnoflag, int level, const char *fmt, va_list ap)
{
	int errno_save, n;
	char buf[MAXLEN];

	errno_save = errno;	/* value caller might want printed */
	vsnprintf(buf, sizeof(buf), fmt, ap);	/* this is safe */
	n = strlen(buf);
	if (errnoflag)
		snprintf(buf + n, sizeof(buf) - n, ": %s", strerror(errno_save));

	fflush(stdout);		/* in case stdout and stderr are the same */
	fprintf(stderr, "%s\n", buf);
	return;
}

void err_msg(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	err_doit(0, LOG_INFO, fmt, ap);
	va_end(ap);
	return;
}

void Debug(const char *fmt, ...)
{
	va_list ap;
	if (debug) {
		va_start(ap, fmt);
		err_doit(0, LOG_INFO, fmt, ap);
		va_end(ap);
	}
	return;
}

void err_sys(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	err_doit(1, LOG_ERR, fmt, ap);
	va_end(ap);
	exit(1);
}

#ifdef STOREMYSQL

#include "storemysql.c"

#endif

/**
 * Open a rawsocket for the network interface
 */
int32_t open_rawsocket(char *ifname)
{
	unsigned char buf[MAX_PACKET_SIZE];
	struct ifreq ifr;
	struct sockaddr_ll sll;

	raw_sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (raw_sockfd == -1)
		err_sys("socket %s - ", ifname);

	// get interface index
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(raw_sockfd, SIOCGIFINDEX, &ifr) == -1)
		err_sys("SIOCGIFINDEX %s - ", ifname);
	raw_ifindex = ifr.ifr_ifindex;

	// get interface mac
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(raw_sockfd, SIOCGIFHWADDR, &ifr) == -1)
		err_sys("SIOCGIFHWADDR %s - ", ifname);
	memcpy(dev_mac, ifr.ifr_hwaddr.sa_data, 6);

	if (debug) {
		int i;
		fprintf(stderr, "MAC_ADDR: ");
		for (i = 0; i < 6; i++) {
			printf("%.2X", (unsigned char)ifr.ifr_hwaddr.sa_data[i]);
			if (i < 5)
				printf(":");
		}
		printf("\n");
	}

	memset(&sll, 0xff, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = htons(ETH_P_ALL);
	sll.sll_ifindex = raw_ifindex;
	if (bind(raw_sockfd, (struct sockaddr *)&sll, sizeof(sll)) == -1)
		err_sys("bind %s - ", ifname);

	/* flush all received packets. 
	 *
	 * raw-socket receives packets from all interfaces
	 * when the socket is not bound to an interface
	 */
	int32_t i, l = 0;
	do {
		fd_set fds;
		struct timeval t;
		FD_ZERO(&fds);
		FD_SET(raw_sockfd, &fds);
		memset(&t, 0, sizeof(t));
		i = select(FD_SETSIZE, &fds, NULL, NULL, &t);
		if (i > 0) {
			recv(raw_sockfd, buf, MAX_PACKET_SIZE, 0);
			l++;
		};
		Debug("interface %d flushed %d packets", raw_ifindex, l);
	}
	while (i > 0);

	Debug("%s opened (fd=%d interface=%d)", ifname, raw_sockfd, raw_ifindex);

	return raw_sockfd;
}

void get_ports(char *s)
{
	char *p = s;
	while (*p) {
		while (*p && (!isdigit(*p)))
			p++;	// skip blank
		if (*p == 0)
			break;
		int port = atoi(p);
		if ((port >= 0) && (port <= 65535))
			Ports[port] = 1;
		while (*p && isdigit(*p))
			p++;	// skip port
	}
}

// function from http://www.bloof.de/tcp_checksumming, thanks to crunsh
u_int16_t tcp_sum_calc(u_int16_t len_tcp, u_int16_t src_addr[], u_int16_t dest_addr[], u_int16_t buff[])
{
	u_int16_t prot_tcp = 6;
	u_int32_t sum = 0;
	int nleft = len_tcp;
	u_int16_t *w = buff;

	/* calculate the checksum for the tcp header and payload */
	while (nleft > 1) {
		sum += *w++;
		nleft -= 2;
	}

	/* if nleft is 1 there ist still on byte left. We add a padding byte (0xFF) to build a 16bit word */
	if (nleft > 0)
		sum += *w & ntohs(0xFF00);	/* Thanks to Dalton */

	/* add the pseudo header */
	sum += src_addr[0];
	sum += src_addr[1];
	sum += dest_addr[0];
	sum += dest_addr[1];
	sum += htons(len_tcp);
	sum += htons(prot_tcp);

	// keep only the last 16 bits of the 32 bit calculated sum and add the carries
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);

	// Take the one's complement of sum
	sum = ~sum;

	return ((u_int16_t) sum);
}

static void set_ip_tcp_checksum(struct iphdr *ip)
{
	struct tcphdr *tcph = (struct tcphdr *)((u_int8_t *) ip + (ip->ihl << 2));
	tcph->check = 0;	/* Checksum field has to be set to 0 before checksumming */
	tcph->check = (u_int16_t) tcp_sum_calc((u_int16_t) (ntohs(ip->tot_len) - ip->ihl * 4),
					       (u_int16_t *) & ip->saddr, (u_int16_t *) & ip->daddr, (u_int16_t *) tcph);

	ip->check = 0;
	unsigned int cksum;
	int idx;
	int odd;
	int len = ip->ihl << 2;
	cksum = 0;

	odd = len & 1;
	len -= odd;
	uint8_t *ptr = (uint8_t *) ip;

	for (idx = 0; idx < len; idx += 2) {
		cksum += ((unsigned long)ptr[idx] << 8) + ((unsigned long)ptr[idx + 1]);
	}

	if (odd) {		/* buffer is odd length */
		cksum += ((unsigned long)ptr[idx] << 8);
	}

	/*
	 * Fold in the carries
	 */

	while (cksum >> 16) {
		cksum = (cksum & 0xFFFF) + (cksum >> 16);
	}
	ip->check = htons(~cksum);

}

void swap_bytes(unsigned char *a, unsigned char *b, int len)
{
	unsigned char t;
	int i;
	if (len <= 0)
		return;
	for (i = 0; i < len; i++) {
		t = *(a + i);
		*(a + i) = *(b + i);
		*(b + i) = t;
	}
}

typedef struct arp_hdr {
	unsigned short ar_hrd;	/* format of hardware address        */
	unsigned short ar_pro;	/* format of protocol address        */
	unsigned char ar_hln;	/* length of hardware address        */
	unsigned char ar_pln;	/* length of protocol address        */
	unsigned short arp_op;	/* ARP opcode (command)                */

	unsigned char arp_sha[ETH_ALEN];	/* sender hardware address        */
	unsigned char arp_sip[4];	/* sender IP address                */
	unsigned char arp_tha[ETH_ALEN];	/* target hardware address        */
	unsigned char arp_tip[4];	/* target IP address                */
} arpheader;

static inline int process_arp(unsigned char *buf, int len)
{
	struct arp_hdr *ah = (struct arp_hdr *)(buf + 14);
#ifdef DEBUGARP
	dump_arp_packet(eh);
#endif
	if (len < (int)(14 + sizeof(struct arp_hdr))) {
#ifdef DEBUGICMP
		printf("len = %d is too small for arp packet??\n", len);
#endif
		return 0;
	}
	if (ntohs(ah->arp_op) != 1) {	// ARP_OP_REQUEST ARP request
		return 0;
	}
	if (memcmp(&my_ip, ah->arp_tip, 4) == 0) {
#ifdef DEBUGARP
		printf("ARP asking me....\n");
#endif
		memcpy((unsigned char *)(buf), (unsigned char *)(buf + 6), 6);
		memcpy((unsigned char *)(buf + 6), (unsigned char *)dev_mac, 6);
		ah->arp_op = htons(2);	// ARP_OP_REPLY
		memcpy(ah->arp_tha, ah->arp_sha, 6);
		memcpy((unsigned char *)&ah->arp_sha, (unsigned char *)dev_mac, 6);
		memcpy(ah->arp_tip, ah->arp_sip, 4);
		memcpy(ah->arp_sip, &my_ip, 4);
#ifdef DEBUGARP
		printf("I will reply following \n");
		dump_arp_packet(eh);
#endif
		struct sockaddr_ll sll;
		memset(&sll, 0, sizeof(sll));
		sll.sll_family = AF_PACKET;
		sll.sll_protocol = htons(ETH_P_ALL);
		sll.sll_ifindex = raw_ifindex;
		int n = sendto(raw_sockfd, buf, len, 0, (struct sockaddr *)&sll, sizeof(sll));
		Debug("want send %d bytes, send out %d", len, n);
	}
	return 0;
}

#define MAXHIS 50

struct {
	time_t tm;
	char fromip[INET_ADDRSTRLEN];
	int fromport;
	char toip[INET_ADDRSTRLEN];
	int toport;
} LogHist[MAXHIS];

int cur_his = 0;

void do_log(char *fromip, int fromport, char *toip, int toport)
{
	time_t tm;
	tm = time(NULL);
	int i;
	int found = 0;
	for (i = 0; i < MAXHIS; i++) {
		if ((tm - LogHist[i].tm < 5)
		    && (fromport == LogHist[i].fromport)
		    && (toport == LogHist[i].toport)
		    && (strcmp(fromip, LogHist[i].fromip) == 0)
		    && (strcmp(toip, LogHist[i].toip) == 0)) {
			found = 1;
			Debug("Found in log his: %d\n", i);
			break;
		}
	}
	if (found)
		return;
	LogHist[cur_his].tm = tm;
	LogHist[cur_his].fromport = fromport;
	LogHist[cur_his].toport = toport;
	strncpy(LogHist[cur_his].fromip, fromip, INET_ADDRSTRLEN);
	strncpy(LogHist[cur_his].toip, toip, INET_ADDRSTRLEN);
	cur_his = (cur_his + 1) % MAXHIS;

	Debug("add to his, now cur_his: %d\n", cur_his);

	fprintf(logfile, "%s %s %d %s %d\n", stamp(), fromip, fromport, toip, toport);
#ifdef STOREMYSQL
	store_mysql(fromip, fromport, toip, toport);
#endif
}

void process_packet(void)
{
	u_int8_t buf[MAX_PACKET_SIZE];
	int len;
	while (1) {
		len = recv(raw_sockfd, buf, MAX_PACKET_SIZE, 0);
		Debug("read from raw %d bytes", len);
		if (len < 56)
			continue;
		unsigned char *packet = buf + 12;
		int pkt_len = len - 12;
		if ((packet[0] == 0x08) && (packet[1] == 0x06)) {	// ARP
			Debug("arp");
			process_arp(buf, len);
			continue;
		}
		if (memcmp(buf, dev_mac, 6)) {
			if (debug)
				Debug("skip not to my mac_addr packets");
			continue;
		}
		if (debug)
			Debug("proto: 0x%02X%02X", packet[0], packet[1]);
		if ((packet[0] == 0x81) && (packet[1] == 0x00)) {	// skip 802.1Q tag 0x8100
			if (debug)
				Debug("skip 802.1Q pkt");
			continue;
		}
		if ((packet[0] == 0x08) && (packet[1] == 0x00)) {	// IPv4 packet 0x0800
			packet += 2;
			pkt_len -= 2;
			struct iphdr *ip = (struct iphdr *)packet;
			if (debug)
				Debug("ipv4 pkt, version=%d, frag=%d, proto=%d, len=%d", ip->version, ntohs(ip->frag_off) & 0x1fff, ip->protocol,
				      ntohs(ip->tot_len));
			if (ip->version != 4) {
				Debug("ip->version is not 4");
				continue;	// check ipv4
			}
			if (ntohs(ip->frag_off) & 0x1fff)
				continue;	// not the first fragment
			if (ip->protocol != IPPROTO_TCP)
				continue;	// not tcp packet
			if (ntohs(ip->tot_len) > pkt_len)
				continue;	// tot_len should < len 

			struct tcphdr *tcph = (struct tcphdr *)(packet + ip->ihl * 4);
//                      Debug("tcp pkt, syn=%d ack=%d", tcph->syn, tcph->ack);
			if (tcph->syn && (!tcph->ack)) {	// SYN packet, send SYN+ACK
				int port = ntohs(tcph->dest);
//                              Debug("tcp syn and not ack, to port %d", port);
				if (Ports[port] == 0) {
					Debug("skip to port %d", port);
					continue;
				}
				swap_bytes((unsigned char *)buf, (unsigned char *)(buf + 6), 6);
				swap_bytes((unsigned char *)&ip->saddr, (unsigned char *)&ip->daddr, 4);
				swap_bytes((unsigned char *)&tcph->source, (unsigned char *)&tcph->dest, 2);
				tcph->ack = 1;
				tcph->ack_seq = htonl(ntohl(tcph->seq) + 1);
				tcph->seq = htonl(MY_SEQ);
				tcph->doff = 20 / 4;
				pkt_len = ip->ihl * 4 + tcph->doff * 4;
				ip->tot_len = htons(pkt_len);
				ip->check = 0;
				set_ip_tcp_checksum(ip);

				struct sockaddr_ll sll;
				memset(&sll, 0, sizeof(sll));
				sll.sll_family = AF_PACKET;
				sll.sll_protocol = htons(ETH_P_ALL);
				sll.sll_ifindex = raw_ifindex;
				int n = sendto(raw_sockfd, buf, pkt_len + 14, 0, (struct sockaddr *)&sll, sizeof(sll));
				Debug("want send %d bytes, send out %d", pkt_len + 14, n);
				continue;
			}
			if (tcph->ack) {	// ACK packet, log
				int port = ntohs(tcph->dest);
//                              Debug("tcp ack, to port %d", port);
				if (Ports[port] == 0) {
//                                      Debug("skip to port %d", port);
					continue;
				}
				if (ntohl(tcph->ack_seq) != MY_SEQ + 1) {
//                                      Debug("ack_seq=%d, not my", ntohl(tcph->ack_seq));
					continue;
				}

				char fromip[INET_ADDRSTRLEN], toip[INET_ADDRSTRLEN];
				snprintf(fromip, INET_ADDRSTRLEN, "%d.%d.%d.%d", IPQUAD(ip->saddr));
				snprintf(toip, INET_ADDRSTRLEN, "%d.%d.%d.%d", IPQUAD(ip->daddr));
				do_log(fromip, ntohs(tcph->source), toip, ntohs(tcph->dest));

				if (do_response == 0)
					continue;
				if (tcph->syn)
					continue;
				if (memcmp((char *)tcph + tcph->doff * 4, "whoisscanme", 11) == 0)	// check loop
					continue;

				swap_bytes((unsigned char *)buf, (unsigned char *)(buf + 6), 6);
				swap_bytes((unsigned char *)&ip->saddr, (unsigned char *)&ip->daddr, 4);
				swap_bytes((unsigned char *)&tcph->source, (unsigned char *)&tcph->dest, 2);
				tcph->ack = 1;
				tcph->ack_seq = tcph->seq;
				tcph->seq = htonl(MY_SEQ + 1);
				tcph->doff = 20 / 4;

				pkt_len = ip->ihl * 4 + tcph->doff * 4 + sprintf((char *)tcph + tcph->doff * 4, "%s:%s\n", "whoisscanme", response_str);
				ip->tot_len = htons(pkt_len);
				ip->check = 0;
				set_ip_tcp_checksum(ip);

				struct sockaddr_ll sll;
				memset(&sll, 0, sizeof(sll));
				sll.sll_family = AF_PACKET;
				sll.sll_protocol = htons(ETH_P_ALL);
				sll.sll_ifindex = raw_ifindex;
				int n = sendto(raw_sockfd, buf, pkt_len + 14, 0, (struct sockaddr *)&sll, sizeof(sll));
				Debug("want send %d bytes, send out %d", pkt_len + 14, n);
				continue;
			}
		}

	}
}

void usage(void)
{
	printf("Usage:\n");
	printf("./whoisscanme [ -d ] [ -n ] [ -r response_str] -i ifname -a my_ipv4 [ -p port1,port2 ]\n");
	printf(" options:\n");
	printf("    -d               enable debug\n");
	printf("    -n               do not send response\n");
	printf("    -r response_str  response str\n");
	printf("    -p port1,port2   tcp ports to monitor, default is all\n");
	printf("    -i ifname        interface to monitor\n");
	printf("    -a my_ipv4       my_ipv4, used do arp\n");
	exit(0);
}

int main(int argc, char *argv[])
{
	int c;
	int user_set_port = 0;
	while ((c = getopt(argc, argv, "dnr:i:a:p:")) != EOF)
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 'n':
			do_response = 0;
			break;
		case 'r':
			strncpy(response_str, optarg, MAXLEN);
			break;
		case 'i':
			strncpy(dev_name, optarg, MAXLEN);
			break;
		case 'a':
			my_ip = inet_addr(optarg);
			break;
		case 'p':
			user_set_port = 1;
			get_ports(optarg);
			break;
		}
	if (dev_name[0] == 0)
		usage();
	if (user_set_port == 0) {
		int i;
		for (i = 0; i < 65536; i++)
			Ports[i] = 1;
	}
	if (debug) {
		printf("         debug = 1\n");
		printf("   do_response = %d\n", do_response);
		printf("  response str = %s\n", response_str);
		printf("         netif = %s\n", dev_name);
		printf("          myip = %s\n", inet_ntoa(*((struct in_addr *)&my_ip)));
	}

	logfile = stdout;

	setvbuf(stdout, NULL, _IONBF, 0);

#ifdef STOREMYSQL
	mysql = connectdb();
#endif

	if (open_rawsocket(dev_name) < 0) {
		perror("Create Error");
		exit(1);
	}

	process_packet();

	return 0;
}
