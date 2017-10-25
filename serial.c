/* -*- Mode: C; tab-width: 8;  indent-tabs-mode: nil; c-basic-offset: 8; c-brace-offset: -8; c-argdecl-indent: 8 -*- */
/* Copyright 2012 Dan Smith <dsmith@danplanet.com> */

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>

#include <fap.h>

#define FEND  0xC0

static int ALARM_INSTALLED = 0;

void alarm_handler(int signal)
{
	printf("IO Timeout\n");
}

int get_packet(int fd, char *buf, unsigned int *len)
{
	unsigned char byte = 0x00;
	char packet[512] = "";
	int ret;
	int pos = 0;
	unsigned int tnc_id;

	if (!ALARM_INSTALLED) {
		struct sigaction action;
		action.sa_handler = alarm_handler;
		action.sa_flags = 0;
		sigemptyset(&action.sa_mask);
		sigaction(SIGALRM, &action, NULL);
		ALARM_INSTALLED = 1;
	}

	alarm(5); /* Five second timeout */

	while (byte != FEND) {
		ret = read(fd, &byte, 1);
		if (ret < 0) {
			printf("TNC read failed: %m\n");
			return 0;
		}
	}

	packet[pos++] = byte;

	while (1 && (pos < sizeof(packet))) {
		ret = read(fd, &byte, 1);
		if (ret != 1)
			continue;
		packet[pos++] = byte;
		if (byte == FEND)
			break;
	}

	alarm(0);

	packet[sizeof(packet)-1] = '\0';
	ret = fap_kiss_to_tnc2(packet, pos, buf, len, &tnc_id);
	if (!ret)
		printf("Failed to convert packet: %s\n", packet);

	return ret;
}

int get_rate_const(int baudrate)
{
	switch (baudrate) {
	case 1200:   return B1200;
	case 4800:   return B4800;
	case 9600:   return B9600;
	case 19200:  return B19200;
	case 38400:  return B38400;
	case 115200: return B115200;
	};

	printf("Unsupported baudrate %i\n", baudrate);

	return B9600;
}

int serial_set_rate(int fd, int baudrate, int hwflow)
{
	struct termios term;
	int ret;

	ret = tcgetattr(fd, &term);
	if (ret < 0)
		goto err;

	cfmakeraw(&term);
	cfsetspeed(&term, get_rate_const(baudrate));

	if (hwflow)
		term.c_cflag |= CRTSCTS;
	else
		term.c_cflag &= ~CRTSCTS;

	ret = tcsetattr(fd, TCSAFLUSH, &term);
	if (ret < 0)
		goto err;

	return 0;
 err:
	perror("unable to configure serial port");
	return ret;
}

int serial_open(const char *device, int baudrate, int hwflow)
{
	int fd;
	int ret;
	struct stat s;

        printf("Opening serial device %s, hang check ... ", device);
        fd = open(device, O_RDWR);
        printf("return 0x%02x\n", fd);
	if (fd < 0)
		return fd;

	fstat(fd, &s);

	if (S_ISCHR(s.st_mode)) {
		ret = serial_set_rate(fd, baudrate, hwflow);
		if (ret) {
			close(fd);
			fd = ret;
		}
	}

	return fd;
}
