/* urlsrv.c
 * Copyright (c) 2007
 * Jeff Nettleton
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <event.h>

/* transaction */
typedef struct _trans_t
{
    unsigned short active; /* 0 or 1 */
    
    /* data */
    char doc_id[64];   /* permanent in system */
    char *host;        /* hostname, for 'Host:' */
    char *url;         /* the url for 'GET' */
    
    /* timer stuff */
    unsigned int ms;   /* ms we're allowing */
    struct timeval tv; /* timeval, to pass to event_add */
    struct event ev;   /* for the timeout */
} trans_t;

typedef struct _conn_t {
    int sock;
    struct sockaddr_in addr;
    struct event ev;

    /* client side */
    char *data;
    unsigned int len, pos;
    trans_t list[512];
} conn_t;

/* the pool of spiders */
static conn_t pool[128];

/* declare our handler functions */

/* accept a new connection */
void accept_new_conn(int, short, void *);
/* receive a message from the spider */
void conn_get_request(int, short, void *);
/* send data back to the spider */
void conn_send_data(int, short, void *);
/* when a url should have already been fetched */
void trans_timed_out(int, short, void *);

/* end declarations */

/**
 * print an error message
 */
void ERRF(const char *file, unsigned int line, const char *format,
    ...)
{
    va_list ap;

#ifdef DEUBG
    assert(NULL != file);
    assert(NULL != format);
#endif

    va_start(ap, format);
    fprintf(stderr, "urlsrv:%s:%u - ERROR: ", file, line);
    vfprintf(stderr, format, ap);
    va_end(ap);
}

/**
 * initialize a connection
 */
int conn_init(conn_t *conn, unsigned short port)
{
    int y = 1;

#ifdef DEBUG
    assert(NULL != conn);
    assert(port > 0);
#endif

    conn->sock = -1;

    if ((conn->sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        /* problems creating the socket */
        ERRF(__FILE__, __LINE__, "creating socket: %s!\n",
            strerror(errno));
        return 0;
    }

    if (setsockopt(conn->sock, SOL_SOCKET, SO_REUSEADDR,
            &y, sizeof y) == -1) {
        /* couldn't clear up a lingering socket */
        ERRF(__FILE__, __LINE__, "clearing socket: %s!\n",
            strerror(errno));
        close(conn->sock);
        return 0;
    }

    /* these aren't portable, so check if they are around 
     * before we start dicking around with setting them.
     */

#ifdef TCP_DEFER_ACCEPT
    /* keep listener sleeping until when data arrives */
    if (setsockopt(conn->sock, IPPROTO_TCP, TCP_DEFER_ACCEPT,
            &y, sizeof y) == -1) {
        /* couldn't set TCP_DEFER_ACCEPT */
        ERRF(__FILE__, __LINE__, "defer accept: %s!\n",
            strerror(errno));
        close(conn->sock);
        return 0;
    }
#endif

#ifdef TCP_NODELAY
    /* since all this does is accept connections */
    if (setsockopt(conn->sock, IPPROTO_TCP, TCP_NODELAY,
            &y, sizeof y) == -1) {
        /* couldn't set TCP_NODELAY */
        ERRF(__FILE__, __LINE__, "nodelay: %s!\n",
            strerror(errno));
        close(conn->sock);
        return 0;
    }
#endif
    
    /* make the socket non-blocking */
    if (fcntl(conn->sock, F_SETFL, O_NONBLOCK) == -1) {
        /* failure */
        ERRF(__FILE__, __LINE__, "non blocking: %s!\n",
            strerror(errno));
        close(conn->sock);
        return 0;
    }

    conn->addr.sin_family = AF_INET;
    conn->addr.sin_port = htons(port);
    conn->addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(conn->addr.sin_zero), '\0', 8);

    /* lets see if we can bind to the port we want */
    if (bind(conn->sock, (struct sockaddr *)&conn->addr,
            sizeof(struct sockaddr)) == -1) {
        ERRF(__FILE__, __LINE__, "bind: %s!\n", strerror(errno));
        close(conn->sock);
        return 0;
    }

    /* lets see if we can listen on this port now */
    if (listen(conn->sock, SOMAXCONN) == -1) {
        ERRF(__FILE__, __LINE__, "listen: %s!\n", strerror(errno));
        close(conn->sock);
        return 0;
    }

    return 1;
}

/**
 * close a socket
 */
void conn_cleanup(conn_t *conn)
{
#ifdef DEBUG
    assert(NULL != conn);
#endif

    if (-1 == conn->sock) {
        /* do nothing. */
        return;
    }

    /* nicer way to close instead of straight disconnect */
    if (shutdown(conn->sock, SHUT_WR)) {
        if (errno != ENOTCONN) {
            ERRF(__FILE__, __LINE__, "shutting down socket(%d)! %s\n",
                conn->sock, strerror(errno));
            return;
        }
    }

    /* if the shutdown wasn't respected... */
    if (-1 != conn->sock) {
        if (close(conn->sock)) {
            ERRF(__FILE__, __LINE__, "closing socket! %s\n",
                strerror(errno));
            return;
        }
    }

    conn->sock = -1;
    memset(&conn->addr, 0, sizeof &conn->addr);
}

/**
 * accept a new connection
 */
void accept_new_conn(int fd, short ev, void *arg)
{
    int tmp_sock, y = 1;
    struct sockaddr_in tmp_addr;
    socklen_t socklen;
    conn_t *spdr;

    /* accept the connection */
    socklen = sizeof tmp_addr;
    if ((tmp_sock = accept(fd, (struct sockaddr *)&tmp_addr,
                &socklen)) == -1) {
        ERRF(__FILE__, __LINE__, "accepting conn: %s!\n",
            strerror(errno));
        return;
    }

    if (tmp_sock >= (signed)sizeof pool) {
        /* too many concurrent connections */
        close(tmp_sock);
        return;
    }
    
    /* make the socket non-blocking */
    if (fcntl(tmp_sock, F_SETFL, O_NONBLOCK) == -1) {
        /* failure */
        ERRF(__FILE__, __LINE__, "non blocking: %s!\n",
            strerror(errno));
        close(tmp_sock);
        return;
    }
    
    /* unset TCP_NODELAY, because that makes shit slower for sends */
#ifdef TCP_CORK    
    if (setsockopt(tmp_sock, IPPROTO_TCP, TCP_CORK,
            &y, sizeof y) == -1) {
        /* couldn't set TCP_CORK */
        ERRF(__FILE__, __LINE__, "cork: %s!\n",
            strerror(errno));
        close(tmp_sock);
        return;
    }
#endif

    /* set up the spdr shit */
    spdr = &pool[tmp_sock];
    spdr->sock = tmp_sock;
    memcpy(&spdr->addr, &tmp_addr, sizeof spdr->addr);

    /* now set up the events */
    event_set(&spdr->ev, spdr->sock, EV_READ | EV_PERSIST,
        conn_get_request, NULL);
    event_add(&spdr->ev, NULL);
}

/**
 * read a spdr's request
 */
void conn_get_request(int fd, short ev, void *arg)
{
    conn_t *spdr;
    ssize_t got;
    char buf[256], *p;
    int d;
    
    spdr = &pool[fd];
    
    /* get rid of our old event for this connection */
    event_del(&spdr->ev);
    
    /* get some more shit nigga! */
    got = recv(spdr->sock, buf, sizeof buf, 0);
        
    if (got == -1) {
        /* erreur! */
        ERRF(__FILE__, __LINE__, "receiving data: %s!\n",
                strerror(errno));
        conn_cleanup(spdr);
        return;
    }
    
    if (sizeof buf == got) {
        /* request the maximum size? fuck you */
        conn_cleanup(spdr);
        return;
    }

    /* parse the request */
    if (NULL == (p = strchr(buf, ' '))) {
        /* there isn't a space after the cmd word */
        ERRF(__FILE__, __LINE__, "bad request '%s'!\n", buf);
        conn_cleanup(spdr);
        return;
    } else {
        /* good request! */
        ++p; /* lose the ' ' */
        p[strlen(p) - 1] = '\0'; /* lose the '\n' */
        d = strtol(p, NULL, 0); /* get our shit */
    }
    
    /* lets find out what they're asking us to dooo! */
    switch (buf[0]) {
        case 'G':
            /* they want to get some url's */
            break;
            
        case 'W':
            /* extend the worker's time */
            if (spdr->list[d].active) {
                /* ok this is for real */
                /* TODO: update spdr->list[d].tv and .ev */
            }

            break;
            
        case 'D':
            /* they're done, remove from our queue */
            if (spdr->list[d].active) {
                /* ok, this was a legitimate transaction */
                spdr->list[d].active = 0;
                /* we'll clear this before its next use */
            }

            break;
            
        default:
            /* they fucked up */
            ERRF(__FILE__, __LINE__, "bad request '%s'!\n", buf);
            conn_cleanup(spdr);
            return;
    }

    /* set up a new handler for writes */
    event_set(&spdr->ev, spdr->sock, EV_WRITE | EV_PERSIST,
        conn_send_data, NULL);
    event_add(&spdr->ev, NULL);
}

/**
 * send the header
 */
void conn_send_data(int fd, short ev, void *arg)
{
    conn_t *spdr;
    ssize_t sent;

    spdr = &pool[fd];

    /* send some of the header */
    while (spdr->pos < spdr->len) {
        if (!(sent = send(spdr->sock, 
                    &spdr->data[spdr->pos], 
                    spdr->len - spdr->pos, 0))) {
            /* failure :'( */
            event_del(&spdr->ev);
            conn_cleanup(spdr);
        } else if (sent == -1) {
            /* errno is set */
            switch (errno) {
                case EAGAIN:
                case EINTR:
                    /* we'll wait and try again */
                    return;
                
                case EPIPE:
                default:
                    /* problem */
                    event_del(&spdr->ev);
                    conn_cleanup(spdr);
                    return;
            }
        }

        /* advance our shit */
        spdr->pos += sent;
    }
    
    if (spdr->pos == spdr->len) {
        /* we finished our sending of the header */
        event_del(&spdr->ev);
        conn_cleanup(spdr);
        return;
    }
}

/**
 * a transaction timed out
 */
void trans_timed_out(int fd, short ev, void *arg)
{
    trans_t *tr;

    tr = (trans_t *) arg;

    /* TODO: everything */

    return;
}

/**
 * lets do this
 */
int main(int argc, char *argv[])
{
    unsigned int i;
    conn_t host;

    /* initialize libevent */
    event_init();
    
    if (!conn_init(&host, 4332)) {
        /* something got fucked up */
        ERRF(__FILE__, __LINE__, "error creating socket on 4332!\n");
        return 1;
    }

    /* set up the accept event */
    event_set(&host.ev, host.sock, EV_READ | EV_PERSIST,
            accept_new_conn, NULL);
    event_add(&host.ev, NULL);

    /* begin our main loop */
    event_dispatch();

    /* clean up (this is unreachable) */
    conn_cleanup(&host);

    return 0;
}
