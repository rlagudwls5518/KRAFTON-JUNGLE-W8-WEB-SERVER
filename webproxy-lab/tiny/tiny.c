/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd); // 하나의 요청과 응답의 과정 함수 
void read_requesthdrs(rio_t *rp); // 헤더의 끝을 찾는 함수
int parse_uri(char *uri, char *filename, char *cgiargs); // 클라가 요청한 uri이 정적 컨텐츠인지 동적 컨텐츠인지 확인하고 CGI환경변수랑 경로 설정하는 함수
void serve_static(int fd, char *filename, int filesize); // 서버에 저장된 정적 컨텐츠를 클라에 응답을 보내는 함수
void get_filetype(char *filename, char *filetype); //파일이름을 조사해서  mime타입 설정
void serve_dynamic(int fd, char *filename, char *cgiargs);// 서버에 저장된 동적컨텐츠를 클라에 응답을 보냄
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);// 클라에러

int main(int argc, char **argv)
{
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
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}

void doit(int fd){
  //1. 클라가 보낸 메소드, 정적파일, HTTP버전을 읽고 분석
  //2. tiny서버는 GET만 지원해서 다른 매소드는 에러메시지 보냄
  //3. 만약 요청헤더가 GET이면 나머지 헤더들을 read_requesthdrs로 읽고 헤더 끝나오면 종료
  //4. parse_uri로 요청라인에서 컨텐츠가 정적인지 동적인지 확인하고 파일 경로와 CGI인자 추출
  //5. 분석한 파일이 실제로 디스크에 존재하는지 확인 없으면 404 에러
  //6. 정적콘텐츠이면  해당파일이 일반파일인지?, 서버가 읽기권한을 가지고 있는지? 확인하고  파일을 내보냄
  //7. 동적컨텐츠이면  해당파일이 일반파일인지? 실행권한을 가지고 있는지? 확인하고 serve_dynamic 실행해서 자식프로세스만들어서 CGI실행해서 내보냄 

  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // 클라이언트에게서 받은 요청
  char filename[MAXLINE], cgiargs[MAXLINE]; // parse_uri를 통해서 채워진다.
  rio_t rio;

  // 클라이언트가 rio로 보낸 request 라인과 헤더를 읽고 분석한다.
  Rio_readinitb(&rio, fd);  // rio 버퍼와 서버의 connfd를 연결시켜준다.
  Rio_readlineb(&rio, buf, MAXLINE);  // rio에 있는 응답라인 한 줄을 모두 buf로 옮긴다.

  printf("Request headers:\n");
  printf("%s", buf);  // 요청 라인 buf = "GET /hi HTTP/1.1\0"을 표준 출력해준다.

  sscanf(buf, "%s %s %s", method, uri, version);  // buf에서 문자열 3개를 읽어와서 method, uri, version이라는 문자열에 저장한다.

  // 요청 method가 GET이 아니면 종료한다. 즉, main으로 가서 연결을 닫고, 다음 요청을 기다린다.
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  // 요청 라인을 뺀 나머지 요청 헤더들은 무시한다.
  read_requesthdrs(&rio);

  // 클라이언트 요청 라인에서 받아온 uri를 이용해서 정적/동적 컨텐츠를 구분한다. 정적 컨텐츠면 1이 저장된다.
  is_static = parse_uri(uri, filename, cgiargs);

  // stat 함수는 file의 상태를 buffer에 넘긴다. 여기서 filename은 클라이언트가 요청한 서버의 컨텐츠 디렉토리 및 파일 이름이다.
  // 여기서 못넘기면 파일이 없다는 뜻이므로, 404 fail이다.
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "서버에 요청하신 파일이 없습니다.");
    return;
  }

  // 컨텐츠의 유형이 정적인지, 동적인지를 파악한 후에 각각의 서버에 보낸다.
  if (is_static) { // 정적 컨텐츠인 경우
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // 일반 파일이 아니거나, 읽을 권한이 없으면
      clienterror(fd, filename, "403", "Forbidden", "권한없는 접근입니다.");
      return;
    }
    // reponse header의 content-length를 위해 정적 서버에 파일의 사이즈를 같이 보낸다
    serve_static(fd, filename, sbuf.st_size);

  } else {  // 동적 컨텐츠인 경우
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {  // 일반 파일이 아니거나, 실행 파일이 아니면
      clienterror(fd, filename, "403", "Forbidden", "권한없는 접근입니다.");
      return;
    }

    // 동적 서버에 인자를 같이 보낸다.
    serve_dynamic(fd, filename, cgiargs);
  }

}

// 클라이언트가 버퍼 rp에 보낸 나머지 요청 헤더들을 무시 -> 헤더의 끝을 찾는?
void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);

  // 버퍼 rp의 마지막 끝을 만날 때까지(Content-Length의 마지막 \r\n) 계속 출력해줘서 없앤다.
  while (strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

// uri를 받아서 요청받은 파일의 이름과 요청 인자를 채워준다.
int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;

  // uri에 cgi-bin이 없다면, 즉 정적 컨텐츠를 요청한다면 1을 반환한다.
  // 예) GET /hi.jpg HTTP/1.1 --> uri에 cgi-bin이 없음

  /*
    static-content. 즉, uri안에 "cgi-bin"과 일치하는 문자열이 없음

    예시)
    uri : /hi.jpg
    ->
    cgiargs :
    filename : ./hi.jpg
  */
  if (!strstr(uri, "cgi-bin")) {
    strcpy(cgiargs, "");    // 동적 컨텐츠인경우 전달할 cgi인자로 들어갈값
    strcpy(filename, ".");  // 현재 경로에서부터 시작한다. ./path ~~
    strcat(filename, uri);  // filename 스트링에 uri 스트링을 이어붙인다.

    // 만약 uri뒤에 '/'이 있으면, 그 뒤에 home.html을 붙인다.
    if (uri[strlen(uri)-1] == '/') {
      strcat(filename, "home.html");
    }

    // 정적 컨텐츠이면 1을 반환한다.
    return 1;
  } 
  /*
    dynamic-content

    예시)
    uri : /cgi-bin/adder?123&123
    ->
    cgiargs : 123&123
    filename : ./cgi-bin/adder
  */
  else {
    ptr = index(uri, '?');

    // '?'이 있으면 cgiargs를 '?' 뒤의 인자들과 값으로 채워주고, '?'를 NULL으로 만든다.
    if (ptr) {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    } else {
      // '?'이 없으면 아무것도 안넣어준다.
      strcpy(cgiargs, "");
    }

    strcpy(filename, '.');  // 현재 디렉토리에서 시작한다.
    strcat(filename, uri);  // uri를 넣어준다.
  }

  return 0;
}


void serve_static(int fd, char *filename, int filesize){
  // 1. 파일 종류를 확인(확장자)
  // 2. 확장자를 바탕으로 컨텐츠 타입이나 길이헤더를 클라에 전송(헤더의 끝을 알리는 빈줄도 같이 전송)
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  //클라이언트에게 응답 헤더 보내기

  get_filetype(filename, filetype);//파일타입 가져오기
  sprintf(buf, "HTTP/1.0 200 OK\r\n");  // 응답 라인 작성하기
  sprintf(buf, "%sServer : Tiny Web Server\r\n", buf);  // 응답 헤더 작성하기
  sprintf(buf, "%sConnection : close\r\n");
  sprintf(buf, "%sContent-Length : %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-Type : %s\r\n", buf, filetype);

  //응답라인과 헤더를 클라이언트에게 보낸다
  Rio_writen(fd, buf, strlen(buf));

  printf("Response headers : \n");
  prinf("%s", buf);


  //클라이언트에게 응답 바디 보내기
  srcfd = Open(filename,O_RDONLY, 0); // filename의 이름을 갖는 파일을 읽기전용으로 열기
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 메모리에 파일 내용을 동적 할당한다.
  Close(srcfd); // 파일을 닫는다.
  Rio_writen(fd, srcp, filesize); // 해당 메모리에 있는 파일 내용들을 fd에 보낸다.(= 읽는다)
  Munmap(srcp, filesize); //메모리 동적할당 해제

}

// Response header의 Conten-Type에 필요한 함수로, filename을 조사해서 각각의 식별자에 맞는 MIME 타입을 filetype에 입력해준다.
void get_filetype(char *filename, char *filetype) {
  if (strstr(filename, ".html")) {
    strcpy(filetype, "text/html");
  } else if (strstr(filename, ".gif")) {
    strcpy(filetype, "image/gif");
  } else if (strstr(filename, ".png")) {
    strcpy(filetype, "image/png");
  } else if (strstr(filename, ".jpg")) {
    strcpy(filetype, "image/jpeg");
  } else if(strstr(filename, ".mpg")){
    strcpy(filetype, "video/mpg");
  } else {
    strcpy(filetype, "text/plain");
  }
}

// 클라이언트가 원하는 동적 컨텐츠 디렉토리를 받아온다. 응답 라인과 헤더를 작성하고 서버에게 보내면, CGI 자식 프로세스를 fork하고, 그 프로세스의 표준 출력을 클라이언트 출력과 연결한다.
void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = { NULL };

  // return first part of HTTP response
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) {
    setenv("QUERY_STRING", cgiargs, 1);

    // 클라이언트의 표준 출력을 CGI 프로그램의 표준 출력과 연결한다. 따라서 앞으로 CGI 프로그램에서 printf하면 클라이언트에서 출력된다.
    Dup2(fd, STDOUT_FILENO);
    Execve(filename, emptylist, environ);
  }
  Wait(NULL);
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  // HTTP response body
  sprintf(body, "<html><title>Tiny Error</title>"
                  "<body bgcolor=\"ffffff\">\r\n"
                  "%s: %s\r\n"
                  "<p>%s: %s\r\n"
                  "<hr><em>The Tiny Web server</em>\r\n",
                  errnum, shortmsg, longmsg, cause);

  // print HTTP response
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));

  sprintf(buf, "Content-Type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));

  sprintf(buf, "Content-Length: %d\r\n\r\n", (int)strlen(body));

  // error msg와 response body를 server socket을 통해 클라이언트에 보낸다.
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

