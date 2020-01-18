#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

#define MAX_PACKET_SIZE 2048
#define MAXLEN 2048

int debug = 0;

int32_t raw_ifindex;
int32_t raw_sockfd;
char dev_name[MAXLEN];
int Ports[65536];

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

void usage(void)
{
	printf("Usage:\n");
	printf("./whoisscanme [ -d ] -i ifname [ -p port1,port2 ]\n");
	printf(" options:\n");
	printf("    -d               enable debug\n");
	printf("    -i ifname        interface to monitor\n");
	printf("    -p port1,port2   tcp ports to monitor\n");
	exit(0);
}

int main(int argc, char *argv[])
{
	int c;
	int user_set_port = 0;
	while ((c = getopt(argc, argv, "di:p:")) != EOF)
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 'i':
			strncpy(dev_name, optarg, MAXLEN);
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
		printf("         netif = %s\n", dev_name);
	}

	setvbuf(stdout, NULL, _IONBF, 0);

	printf("open rawsocket\n");
	if (open_rawsocket(dev_name) < 0) {
		perror("Create Error");
		exit(1);
	}

	process_packet();

	return 0;
}
