/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/* HTTP 요청 한 개를 읽고, 정적/동적 여부를 판단해 알맞게 응답 */
void doit(int fd)
{ 
    printf("[doit] start\n");
    int is_static; 
    struct stat sbuf; 
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE]; 
    rio_t rio; 

    Rio_readinitb(&rio, fd); 
    Rio_readlineb(&rio, buf, MAXLINE); 
    printf("Request headers:\n");
    printf("%s", buf); 
    sscanf(buf, "%s %s %s", method, uri, version); 
    if (strcasecmp(method, "GET")) {
      clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
      return;
    }

    read_requesthdrs(&rio); 

    is_static = parse_uri(uri, filename, cgiargs); 
    if (stat(filename, &sbuf) < 0) {
      clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
      return; 
    }

    if (is_static) {
      if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
        return; 
      } 
      serve_static(fd, filename, sbuf.st_size);
    }    
    else {
      if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
        clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
        return; 
      }
      serve_dynamic(fd, filename, cgiargs); 
    }
}

/* 요청 라인 뒤에 HTTP 헤더를 빈 줄까지 읽어 소비 */
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];
    
    Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {
      Rio_readlineb(rp, buf, MAXLINE);
      printf("%s", buf); 
    }
    return; 
}

/* URI를 분석해서 정적 요청인지 동적 요청인지 구분하고 경로를 만듦*/
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    printf("[parse_uri] enter parse_uri: %s\n", uri);
    char *ptr; 

    if (!strstr(uri, "cgi-bin")) {
      strcpy(cgiargs, "");
      strcpy(filename, ".");
      strcat(filename, uri);

      if (uri[strlen(uri)-1] == '/') {
        strcat(filename, "home.html");
      }

      return 1; 
    }
    else {
      ptr = index(uri, '?'); 
      if (ptr) {
        strcpy(cgiargs, ptr+1); 
        *ptr = '\0'; 
      } 
      else {
        strcpy(cgiargs, "");
      }
      strcpy(filename, ".");
      strcat(filename, uri);
      return 0; 
    }
}

/* 정적 파일에 대한 HTTP 헤더와 파일 내용을 전송 */
void serve_static(int fd, char *filename, int filesize)
{
    printf("serve_static: %s\n", filename);
    int srcfd; 
    char *srcp, filetype[MAXLINE], buf[MAXBUF]; 

    /* Send response headers to client */
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf); 
    sprintf(buf, "%sConnection: close\r\n", buf); 
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize); 
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}

void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html")) {
      strcpy(filetype, "text/html");
    } 
    else if (strstr(filename, ".gif")) {
      strcpy(filetype, "image/gif");
    }
    else if (strstr(filename, ".png")) {
      strcpy(filetype, "image/png");
    }
    else if (strstr(filename, ".jpg")) {
      strcpy(filetype, "image/jpeg");
    }
    else {
      strcpy(filetype, "text/plain");
    }
}

/* CGI 프로그램을 실행해 동적 응답을 생성 */
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
    printf("serve_dynamic: %s\n", filename);
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if (Fork() == 0) {
      setenv("QUERY_STRING", cgiargs, 1);
      Dup2(fd, STDOUT_FILENO);
      Execve(filename, emptylist, environ);
    }
    Wait(NULL);
}

/* HTTP 에러 상태와 HTML 에러 페이지를 만들어 전송 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body); 
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg); 
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));

}
            
/* 서버 소켓을 열고, 연결을 하나씩 받아 doit()에 넘긴 뒤 닫는 반복 루프 */
int main(int argc, char **argv)
{
  printf("[main] main started\n");
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);

  while (1)
  {
    clientlen = sizeof(clientaddr);


    // 클라이언트가 들어올 때까지 기다렸다가, 들어오면 그 연결을 받아서 통신용 소켓을 하나 만들어 주는 함수 
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept

    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("[main] Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}


