#include "../csapp.h"

/*
    이 코드는 client socket interface의 동작 원리를 구현한 코드이다.
    client socket interface는 다음과 같은 순서로 진행된다:
        
    getaddrinfo() -> socket() -> connect() -> rio_writen() -> rio_readlineb() -> close()
*/
int main(int argc, char **argv)
{
    int clientfd; 

    // host: 접속할 서버 호스트 이름 또는 IP 주소 
    // port: 접속할 서버 포트 번호 
    // buf: 데이터를 임시로 저장할 버퍼
    char *host, *port, buf[MAXLINE]; 
    rio_t rio; // rio_readlineb() 같은 함수를 사용할때 쓰는 구조체 변수 

    // 예시 input: ./client localhost 8080
    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(1);
    }
    host = argv[1]; 
    port = argv[2]; 

    // getaddrinfo() -> socket() -> connect() 과정을 수행 
    clientfd = Open_clientfd(host, port);

    Rio_readinitb(&rio, clientfd); 

    // 표준 입력에서 한 줄씩 읽음, EOF가 나오면 반복 종료 
    while (Fgets(buf, MAXLINE, stdin) != NULL) 
    {
        Rio_writen(clientfd, buf, strlen(buf)); // 사용자가 입력한 한 줄을 서버로 전송 
        Rio_readlineb(&rio, buf, MAXLINE); // 서버가 보낸 응답 한 줄을 읽어서 buf에 저장 
        Fputs(buf, stdout); // 서버로부터 받은 응답을 화면에 출력 
    }
    Close(clientfd); // 서버와 연결된 소켓 닫기
    exit(0); // 프로그램 정상 종료 
}