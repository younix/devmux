/*
 * Copyright (c) 2013 Jan Klemkow <j.klemkow@wemelug.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

void
usage(void)
{
	fprintf(stderr, "devmux [-d <device>] [-i <file>] [-o <file>]\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char**argv)
{
	/* file and device stuff */
	char *dev_path = "/dev/l4pipe0";
	char *in_path = "in";
	char *out_path = "out";
	int dev_fd, in_fd, out_fd;

	/* internal buffer stuff */
	char in_buf[BUFSIZ];
	char out_buf[BUFSIZ];
	ssize_t in_buf_size = 0;
	ssize_t out_buf_size = 0;
	ssize_t in_off = 0;
	ssize_t out_off = 0;

	/* event management stuff */
	struct kevent kev[3];
	int ch, kq;
	struct kevent *in_queue = &kev[0];
	struct kevent *out_queue = &kev[1];
	struct kevent *changelist = &kev[0];
	struct kevent *event = &kev[2];

	while ((ch = getopt(argc, argv, "d:i:o:")) != -1) {
		switch (ch) {
		case 'd':
			if ((dev_path = strdup(optarg)) == NULL)
				err(EXIT_FAILURE, "strdup");
			break;
		case 'i':
			if ((in_path = strdup(optarg)) == NULL)
				err(EXIT_FAILURE, "strdup");
			break;
		case 'o':
			if ((out_path = strdup(optarg)) == NULL)
				err(EXIT_FAILURE, "strdup");
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (mkfifo("in", S_IRUSR | S_IWUSR) < 0)
		err(EXIT_FAILURE, "mkfifo");

	if (mkfifo("out", S_IRUSR | S_IWUSR) < 0)
		err(EXIT_FAILURE, "mkfifo");

	if ((dev_fd = open(dev_path, O_RDWR)) < 0)
		err(EXIT_FAILURE, "open");

	if ((in_fd = open(in_path, O_RDONLY)) < 0)
		err(EXIT_FAILURE, "open");

	if ((out_fd = open(out_path, O_WRONLY)) < 0)
		err(EXIT_FAILURE, "open");

	if ((kq = kqueue()) < 0)
		err(EXIT_FAILURE, "kqueue");

	for(;;) {
		/* define events for this round */
		if (in_buf_size == 0)	
			EV_SET(in_queue, in_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
		else
			EV_SET(in_queue, dev_fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);

		if (out_buf_size == 0)	
			EV_SET(out_queue, dev_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
		else
			EV_SET(out_queue, out_fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);

		/* waiting for kernel events */
		if (kevent(kq, changelist, 2, event, 1, NULL) < 0)
			err(EXIT_FAILURE, "kevent");

		/* read data from device */
		if (event->ident == dev_fd && event->filter == EVFILT_READ) {
			out_buf_size = read(dev_fd, out_buf, BUFSIZ);
			out_off = write(out_fd, out_buf, in_buf_size);
			out_buf_size -= out_off;
		}

		/* write data to device */
		if (event->ident == dev_fd && event->filter == EVFILT_WRITE) {
			in_off += write(dev_fd, in_buf, in_buf_size);
			in_buf_size -= in_off;
		}

		/* read data from in file */
		if (event->ident == in_fd && in_buf_size == 0) {
			in_buf_size = read(in_fd, in_buf, BUFSIZ);
			in_off = write(dev_fd, in_buf, in_buf_size);
			in_buf_size -= in_off;
		}

		/* write data to out file */
		if (event->ident == out_fd) {
			out_off += write(out_fd, out_buf + out_off, out_buf_size);
			out_buf_size -= out_off;
		}
	}

	close(dev_fd);
	close(in_fd);
	close(out_fd);

	return EXIT_SUCCESS;
}
