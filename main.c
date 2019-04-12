#include <sys/socket.h>    
#include <sys/wait.h>    
#include <netinet/in.h>    
#include <netinet/tcp.h>    
#include <sys/epoll.h>    
#include <sys/sendfile.h>    
#include <sys/stat.h>    
#include <unistd.h>    
#include <stdio.h>    
#include <stdlib.h>    
#include <string.h>    
#include <strings.h>    
#include <fcntl.h>    
#include <errno.h>   
#include "list.h"
#include <signal.h>

#define	SIGINT		2	/* Interactive attention signal.  */

static int	nchildren;
static pid_t *pids;

#define MAX_EVENTS 100    
#define PORT 8080  

typedef	void	Sigfunc(int);	/* for signal handlers */

void Sigemptyset(sigset_t *set){
	if (sigemptyset(set) == -1){
		perror("sigemptyset error");
    }
}

Sigfunc * signal(int signo, Sigfunc *func){
	struct sigaction act, oact;
	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (signo == SIGALRM) {
#ifdef	SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT;	/* SunOS 4.x */
#endif
	} else {
#ifdef	SA_RESTART
		act.sa_flags |= SA_RESTART;		/* SVR4, 44BSD */
#endif
	}
	if (sigaction(signo, &act, &oact) < 0)
		return(SIG_ERR);
	return(oact.sa_handler);
}

Sigfunc * Signal(int signo, Sigfunc *func){
	Sigfunc	*sigfunc;
	if ( (sigfunc = signal(signo, func)) == SIG_ERR)
		perror("signal error");
	return(sigfunc);
}

void sig_int(int signo){
	/* terminate all children */
	for (int i = 0; i < 4; i++){
		kill(pids[i], SIGTERM);
    }
	while (wait(NULL) > 0){
    };	
	if (errno != ECHILD){
        perror("wait error");
    }
	exit(0);
}

//设置socket连接为非阻塞模式    
void setnonblocking(int sockfd) {    
    int opts;    
    opts = fcntl(sockfd, F_GETFL);    
    if(opts < 0) {    
        perror("fcntl(F_GETFL)\n");    
        exit(1);    
    }    
    opts = (opts | O_NONBLOCK);    
    if(fcntl(sockfd, F_SETFL, opts) < 0) {    
        perror("fcntl(F_SETFL)\n");    
        exit(1);    
    }    
}    

//1.创建链表
typedef struct sockClass {
    int cliSock;
    char cliBuf[BUFSIZ];
    struct list_head node_sockClass;
} sockClass_t;

sockClass_t *make_sockClass(int cli, char *bf) {
    sockClass_t* sock = NULL;
    if ((sock = calloc(1,sizeof(struct sockClass))) == NULL) {
        return NULL;
    }
    if (strlen(bf) > (BUFSIZ-1)) {
        return NULL;
    }
    strcpy(sock->cliBuf,bf);
    sock->cliSock = cli;
    return sock;
}

void child_main(int i, int listenfd, int addrlen){

    int nfds,nread,n,fd,conn_sock;
    struct epoll_event ev, events[MAX_EVENTS];   
    struct sockaddr_in local, remote;    
    int lenaddr = sizeof(struct sockaddr_in);

    LIST_HEAD(class);
    int epfd = epoll_create(MAX_EVENTS);    
    if (epfd == -1) {    
        perror("epoll_create\n");    
        exit(EXIT_FAILURE);    
    }      
    ev.events = EPOLLIN;    
    ev.data.fd = listenfd;    
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) == -1) {    
        perror("epoll_ctl: listen_sock\n");    
        exit(EXIT_FAILURE);    
    }    
    
    for (;;) {    
        nfds = epoll_wait(epfd, events, MAX_EVENTS, -1); 
        printf("epoll_wait--nfds：%d -pid:%d------\n",nfds,i);   
       if (nfds == -1) {    
            perror("epoll_pwait\n");    
            exit(EXIT_FAILURE);    
        }    
        for (int i = 0; i < nfds; ++i) {    
            fd = events[i].data.fd;  
            if (fd == listenfd) {  
                printf("conn_sock1111:%d\n",conn_sock);      
                while ((conn_sock = accept(listenfd,(struct sockaddr *)&remote,&lenaddr)) > 0) {    
                    setnonblocking(conn_sock); //设置连接socket为非阻塞   
                    printf("conn_sock:%d\n",conn_sock);    
                    ev.events = EPOLLIN | EPOLLET; //边沿触发要求套接字为非阻塞模式；水平触发可以是阻塞或非阻塞模式   
                    ev.data.fd = conn_sock;    
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, conn_sock,&ev) == -1) {    
                        perror("epoll_ctl: add\n");    
                        exit(EXIT_FAILURE);    
                    }    
                }    
                if (conn_sock == -1) {    
                    if (errno != EAGAIN && errno != ECONNABORTED && errno != EPROTO && errno != EINTR) {
                        perror("accept\n");
                    }    
                }    
                continue;    
            }      
            if (events[i].events & EPOLLIN) {
                char buf[BUFSIZ];        
                n = 0;    
                while ((nread = read(fd, buf + n, BUFSIZ-1)) > 0) {    
                    n += nread;    
                }    
                if (nread == -1 && errno != EAGAIN) {    
                    perror("read error\n");    
                } 
                ev.data.fd = fd;    
                ev.events = EPOLLIN | EPOLLOUT | EPOLLET;   
                if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == -1) {    
                    perror("epoll_ctl: mod\n");    
                }  
                printf("===>read：%s------\n",buf);
                
                //把buf按照id加入到链表中
                sockClass_t *st = make_sockClass(fd,buf);
                if (st) {
                   list_add_tail(&st->node_sockClass,&class);
                }else{
                    perror("sockClass节点创建失败\n");
                }
            }    
            if (events[i].events & EPOLLOUT) {  
                perror("===>write：------\n"); 
                sockClass_t *st;
                sockClass_t *tmp;
                if (list_empty(&class)) {
                    perror("list为空\n");
                }else{
                    list_for_each_entry_safe(st,tmp,&class,node_sockClass){
                        if (st->cliSock == fd) {
                            sprintf(st->cliBuf, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\nHello World", 11);    
                            int nwrite, data_size = strlen(st->cliBuf);    
                            n = data_size;    
                            while (n > 0) {    
                                nwrite = write(fd, st->cliBuf + data_size - n, n);    
                                if (nwrite < n) {    
                                    if (nwrite == -1 && errno != EAGAIN) {    
                                        perror("write error\n");    
                                    }    
                                    break;    
                                }    
                                n -= nwrite;    
                            }
                            list_del(&st->node_sockClass);
                            free(st);
                            close(fd);    
                        }
                    }
                }
            }    
        }    
    } 
	close(epfd);
    close(listenfd); 
    exit(0);
}

pid_t Fork(void){
	pid_t pid;
	if ( (pid = fork()) == -1){
        perror("fork error\n");    
    }
	return(pid);
}

pid_t child_make(int i, int listenfd, int addrlen){
	pid_t pid;
	if ((pid = Fork()) > 0){
        return(pid);		/* parent */
    }
	child_main(i, listenfd, addrlen);	/* never returns */
}
void *Calloc(size_t n, size_t size){
	void *ptr;
	if ( (ptr = calloc(n, size)) == NULL){
		perror("calloc error");
    }
	return(ptr);
}

int main(){    
    struct epoll_event ev, events[MAX_EVENTS];    
    int addrlen, listenfd, conn_sock, nfds, fd, i, nread, n;    
    struct sockaddr_in local, remote;    

    //创建listen socket    
    if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {    
        perror("sockfd\n");    
        exit(1);    
    }    
    setnonblocking(listenfd);    
    bzero(&local, sizeof(local));    
    local.sin_family = AF_INET;    
    local.sin_addr.s_addr = htonl(INADDR_ANY);;    
    local.sin_port = htons(PORT);    
    if( bind(listenfd, (struct sockaddr *) &local, sizeof(local)) < 0) {    
        perror("bind\n");    
        exit(1);    
    }    
    listen(listenfd, 1024);    
    //创建cpu核心数的进程
    pids = Calloc(4, sizeof(pid_t));
	for (int i = 0; i < 4; i++){
        printf("--fork---:%d",i);
		pids[i] = child_make(i, listenfd, addrlen);	/* parent returns */
    }
	Signal(SIGINT, sig_int);
    for(;;){
       pause();
    }
}

