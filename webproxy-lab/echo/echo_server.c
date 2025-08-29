#include <stdio.h>
#include "csapp.h"

int open_listenfd(char *port);
void echo(int connfd);

int main(int argc, char **argv) {
	// 여기에 코드를 작성하세요
    exit(0);
}

int open_listenfd(char *port) {
    int listenfd, optval = 1;    // 서버 소켓 디스크립터, 소켓 옵션 설정용 변수
    struct addrinfo hints, *listp, *p;  // 주소 정보 힌트와 결과 리스트 포인터들

    // hints 초기화: 어떤 종류의 주소 정보를 원하는지 설정
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;     // TCP(연결 지향형 스트림)
    hints.ai_flags = AI_NUMERICSERV;     // 포트가 숫자 문자열이라는 의미 ("80" 등)
    hints.ai_flags |= AI_ADDRCONFIG;     // 현재 시스템의 IPv4/IPv6 환경을 고려
    // NULL을 넘겨서 "모든 IP 주소"에 바인딩 (서버의 모든 네트워크 인터페이스에서 접속 허용)
    Getaddrinfo(NULL, port, &hints, &listp);

    // 주소 리스트를 하나씩 돌면서 소켓 생성 + 바인딩 시도
    for (p = listp; p != NULL; p = p->ai_next) {

        // 소켓 생성
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            continue;  // 실패 시 다음 주소 시도
        }

        // 포트 재사용 설정 (서버 재시작 시 "Address already in use" 방지)
        Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));

        // 바인딩 시도 (서버 소켓에 IP/포트 연결)
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) {
            break; // 바인딩 성공
        }

        // 바인딩 실패 → 소켓 닫고 다음 주소 시도
        Close(listenfd);
    }

    // 주소 리스트 메모리 해제
    Freeaddrinfo(listp);

    // 모든 바인딩 시도 실패
    if (p == NULL) {
        return -1;
    }

    // 클라이언트의 연결 요청을 대기 상태로 설정
    if (listen(listenfd, LISTENQ) < 0) {
        // 실패 시 소켓 닫고 -1 반환
        Close(listenfd);
        return -1;
    }

    // 성공 시, 클라이언트 연결을 받을 수 있는 서버 소켓 디스크립터 반환
    return listenfd;
}

void echo(int connfd) {
    // 여기에 코드를 작성하세요
}