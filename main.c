#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifndef INBUFSIZE
#define INBUFSIZE 4096
#endif

#ifndef INIT_SESSION_ARRAY_SIZE
#define INIT_SESSION_ARRAY_SIZE 32
#endif

#ifndef MAX_LISTEN_QUEUE
#define MAX_LISTEN_QUEUE 32
#endif

#ifndef TEMPBUFSIZE
#define TEMPBUFSIZE 46
#endif

int global_var_of_server = 0;

enum commands { up, down, show, unknown };

struct session {
	int fd;
	unsigned int ip;
	unsigned short port;
	char buf[INBUFSIZE];
	int buf_used;
};

static void session_send_string(struct session *sess, const char *str)
{
	write(sess->fd, str, strlen(str));
}

static struct session *make_new_session(int fd, struct sockaddr_in *addr)
{
	struct session *sess = malloc(sizeof(*sess));
	sess->fd = fd;
	sess->ip = ntohl(addr->sin_addr.s_addr);
	sess->port = ntohs(addr->sin_port);
	sess->buf_used = 0;
	return sess;
}

static void exec_command(struct session *sess, enum commands command)
{
	char temp_buf[TEMPBUFSIZE];
	switch(command) {
		case up:
			global_var_of_server++;
			session_send_string(sess, "OK\n");
			break;
		case down:
			global_var_of_server--;
			session_send_string(sess, "OK\n");
			break;
		case show:
			sprintf(temp_buf, "Server var is %d\n", global_var_of_server);
			session_send_string(sess, temp_buf);
			break;
		case unknown:
			session_send_string(sess, "Unknown command\n");
	}
}

static int lenght_word(const char *str)
{
	int len;
	for(len = 0; str[len] >= 97 && str[len] <= 122; len++)
		{}
	return len;
}

static int words_in_string(const char *str)
{
	int wcnt = 0, pr = -1, i;
	for(i = 0; str[i] != '\n'; i++) {
		if(str[i] >= 97 && str[i] <= 122) {
			if(pr == -1) {
				wcnt++;
				pr = 0;
			}
		} else 
			pr = -1;
	}
	return wcnt;
}

static enum commands command_from_string(const char *str)
{
	int len, wcnt, res, i;
	wcnt = words_in_string(str);
	if(wcnt == 0 || wcnt > 1)
		return unknown;
	for(i = 0; str[i] != '\n'; i++) {
		if(str[i] >= 97 && str[i] <= 122) {
			len = lenght_word(str+i);
			if(len < 2)
				return unknown;
			if(i >= len - 1)
				return unknown;
			res = strncmp(str+i, "up", 2);
			if(res != 0) {
				res = len - i;
				if(res != 4)
					return unknown;
				res = strncmp(str+i, "down", 4);
				if(res != 0) {
					res = strncmp(str+i, "show", 4);
					if(res != 0)
						return unknown;
					else
						return show;
				} else
					return down;
			} else {
				if(len == 2)
					return up;
			}
			return unknown;
		}
	}
	return unknown;
}

static void cleanup_part_of_buf(struct session *sess, int end)
{
	if(sess->buf_used - end == 0)
		memset(sess->buf, 0, end);
	else
		memmove(sess->buf, sess->buf+end, sess->buf_used - end);
}

static void check_buf(struct session *sess)
{
	enum commands command;
	int i;
	for(i = 0; i < sess->buf_used; i++) {
		if(sess->buf[i] == '\n') {
			command = command_from_string(sess->buf);
			exec_command(sess, command);
			cleanup_part_of_buf(sess, i+1);
			sess->buf_used -= ++i;
			i = 0;
		}
	}
}

static int session_do_read(struct session *sess)
{
	int rc, bufp = sess->buf_used;
	rc = read(sess->fd, sess->buf + bufp, INBUFSIZE-bufp);
	if(rc <= 0)
		return 0;
	sess->buf_used += rc;
	check_buf(sess);
	if(sess->buf_used >= INBUFSIZE) {
		session_send_string(sess, "Error. Line is too long.\n");
		return 0;
	}
	return 1;
}

/*		SERVER		*/
struct server_str {
	int ls;
	int port;
	struct session **sessions_array;
	int session_array_size;
};

static int server_init(struct server_str *serv, long port)
{
	int sd, res, i, opt;
	struct sockaddr_in addr;

	sd = socket(AF_INET, SOCK_STREAM, 0);
	if(sd == -1) {
		perror("socket()");
		return 0;
	}
	opt = 1;
	setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	res = bind(sd, (struct sockaddr *) &addr, sizeof(addr));
	if(res == -1) {
		perror("bind()");
		return 0;
	}
	
	listen(sd, MAX_LISTEN_QUEUE);
	
	serv->ls = sd;
	serv->port = port;
	
	serv->session_array_size = INIT_SESSION_ARRAY_SIZE;
	serv->sessions_array = 
		malloc(INIT_SESSION_ARRAY_SIZE*sizeof(*serv->sessions_array));
	for(i = 0; i < INIT_SESSION_ARRAY_SIZE; i++)
		serv->sessions_array[i] = NULL;
	
	return 1;
}

static void server_accept_client(struct server_str *serv)
{
	int fd, i;
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	fd = accept(serv->ls, (struct sockaddr *)&addr, &len);
	if(fd == -1) {
		perror("accept()");
		return;
	}
	
	if(fd >= serv->session_array_size) {
		int newlen = serv->session_array_size;
		while(fd >= newlen)
			newlen += INIT_SESSION_ARRAY_SIZE;
		serv->sessions_array = 
			realloc(serv->sessions_array, newlen * sizeof(struct session*));
		for(i = serv->session_array_size; i < newlen; i++)
			serv->sessions_array[i] = NULL;
		serv->session_array_size = newlen;
	}
	serv->sessions_array[fd] = make_new_session(fd, &addr);
}

static void server_close_connection(struct session *sess)
{
	close(sess->fd);
	free(sess);
}

static int server_go(struct server_str *serv)
{
	fd_set readfds;
	int maxd, i, sr, ssr;
	for(;;) {
		FD_ZERO(&readfds);
		FD_SET(serv->ls, &readfds);
		maxd = serv->ls;
		for(i = 0; i < serv->session_array_size; i++) {
			if(serv->sessions_array[i]) {
				FD_SET(serv->sessions_array[i]->fd, &readfds);
				if(serv->sessions_array[i]->fd > maxd)
					maxd = serv->sessions_array[i]->fd;
			}
		}
		sr = select(maxd+1, &readfds, NULL, NULL, NULL);
		if(sr == -1) {
			perror("select()");
			return 0;
		}
		if(FD_ISSET(serv->ls, &readfds))
			server_accept_client(serv);
		for(i = 0; i < serv->session_array_size; i++) {
			if(serv->sessions_array[i] && 
				FD_ISSET(serv->sessions_array[i]->fd, &readfds)) {
				ssr = session_do_read(serv->sessions_array[i]);
				if(!ssr) {
					server_close_connection(serv->sessions_array[i]);
					serv->sessions_array[i] = NULL;
				}
			}
		}
	}
	return 1;
}


int main(int argc, char **argv)
{
	struct server_str server;
	long port;
	char *endptr;
	int res;

	if(argc != 2) {
		fprintf(stderr, "Incorrect args\n");
		return 1;
	}
	port = strtol(argv[1], &endptr, 10);
	if(!*argv[1] || *endptr) {
		fprintf(stderr, "Invalid port number\n");
		return 2;
	}
	
	if(!server_init(&server, port)) {
		fprintf(stderr, "server has't init\n");
		return 3;
	}
	
	res = server_go(&server);

	return res ? 0 : 4;
}
