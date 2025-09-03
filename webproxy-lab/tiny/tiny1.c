#include "csapp.h"


//서버는 뭘로 클라를 여는가 -> 포트번호

void read_requesthdrs(rio_t *rp);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
int parse_uri(char *uri, char *filename, char *cgiargs);
void read_requesthdrs(rio_t *rp);
void doit(int fd);
void serve_static(int fd, char *filename, int filesize, int flag);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);

int main(int argc, char *argv){

    int listenfd, confd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    //일단 서버열때 인자를 넣어야하는디 포트번호를 안 넣는경우도 처리

    if(argc != 2){//인자갯수가 2개가 아닐때
        fprint(stderr, "usage : %s <port>\n", argv[0]);
    }
    //포트를 받았으면 그다음은?> 
    //듣기소켓 열기

    listenfd = Open_listenfd(argv[1]);// 연결요청을 받을 준비가 된 듣기소켓에 포트번호 넘겨주기

    //그 다음은 무한정 기다리기?
    //accept로 계속 기다리기

    while(1){
        confd = accept(listenfd, (SA *)&clientaddr, &clientlen);

        printf("connected");

        Close(confd);
    }
}

//connfd로 들어온 HTTP 요청 메시지를 읽어서 터미널에 출력한다.
void doit(int fd){

    struct stat sbuf;
    int flag, is_static;
    rio_t rio;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];

    //클라이언트가 요청 헤더를 읽고 분석
    Rio_readinitb(&rio,fd);
    Rio_readlineb(&rio,fd, MAXLINE);

    printf("Request headers:\n");
    printf("%s",buf);

    sscanf(buf, "%s %s %s", method, uri, version);

    if(strcasecmp(method, "GET") == 0){
        flag = 1;
    }

    else if (strcasecmp(method, "HEAD") == 0){
        flag = 0;
    }

    else{
        clenterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
        return;
    }

    read_requesthdrs(&rio);// 헤더의 끝까지 읽고 빈줄나오면 끝냄

    // 요청라인에서 uri로 동적컨텐츠인지 정적컨텐츠인지 구분하는 함수
    is_static = parse_uri(uri, filename, cgiargs);


    //파일의 정보를 알아내는 시스템 함수 보통 성공하면 0 실패하면 0보다 작은값
    //보통 실패하면 파일이 없을때
    if(stat(filename, &sbuf) < 0){
        clienterror(fd, filename, "404", "Not found", "서버에 요청하신 파일이 없습니다.");
        return;
    }


    //정적컨텐츠인지 동적컨텐츠인지 확인해서 서버로 보내기

    if(is_static){//정적 컨텐츠
        //일반파일이 아니거나 읽기전용이 아니면
        if(!(S_IREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
            clenterror(fd, filename, "403", "Forbidden", "권한없는 접근입니다.");
            return;
        }
        serve_static();
    }
    else{//동적 컨텐츠
        //똑같이 일반파일이 아니거나 동적 컨텐츠니까 실행파일이 아니면
        if(!(S_IREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
           clenterror(fd, filename, "403", "Forbidden", "권한없는 접근입니다.");
           return;
        }
        serve_dynamic();
    }
}

void read_requesthdrs(rio_t *rp){// 요청 헤더 끝까지 읽기(빈줄이 나올때까지)

    char buf[MAXLINE];
    Rio_readlineb(rp, buf, MAXLINE);

    // 읽어오는 값이 남아있어서 0이상이면서 읽어온줄이 \r\n이 아닐동안 0이 아니니까 
    while(Rio_readlineb(rp, buf, MAXLINE) > 0 && strcmp(buf, "\r\n")){
        printf("%s",buf);
    }
    return;
}

//정적 컨텐츠일때랑 
//동적 컨텐츠일때 
//어떻게 구분하나 cgi/bin으로 구분? 
//나중에 동적컨텐츠들은 한폴더에 몰아넣고 폴더가uri에 있을때랑 없을때를 구분지으면 되겠네
//strstr 문자열내 검색만
//strcpy 문자열을 다른문자열, 변수에 복사(덮어쓰는거기때문에 버퍼 오버플로우 날 수도)
//strcat 문자열과 다른문자열을 붙이는 역할
int parse_uri(char *uri, char *filename, char *cgiargs){
    char * ptr;
    
    
    if(!strstr(uri,"cgi-bin")){//정적컨텐츠
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);

        if(uri[strlen(uri)-1] == '/'){
            strcat(filename, "home.html");
        }
        return 1;

    }
    else{//동적컨텐츠
        ptr = index(uri,'?');

        if(ptr){
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        }
        else{
            strcpy(cgiargs, "");
        }

        strcpy(filename, ".");
        strcat(filename, uri);

    }
    return 0;
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
      char buf[MAXLINE], body[MAXBUF];

  // HTTP response body
  sprintf(body, "<html><title>Tiny Error</title>"
                  "<body bgcolor=\"ffffff\">\r\n"
                  "%s: %s\r\n"
                  "<p>%s: %s\r\n"
                  "<hr><em>The Tiny Web server</em>\r\n",
                  errnum, shortmsg, longmsg, cause);

  // print HTTP response
  sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));

  sprintf(buf, "Content-Type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));

  sprintf(buf, "Content-Length: %d\r\n\r\n", (int)strlen(body));

  // error msg와 response body를 server socket을 통해 클라이언트에 보낸다.
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void serve_static(int fd, char *filename, int filesize, int flag){

    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];


    //클라에게 응답 헤더 보내는 과정

    get_filetype(filename, filetype);// 파일 타입 가져옴
    // 첫 줄은 버퍼의 시작주소부터 작성
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    // 두 번째 줄부터는 이전에 쓴 내용의 끝에 이어 붙임
    sprintf(buf + strlen(buf), "Server: Tiny Web Server\r\n");
    sprintf(buf + strlen(buf), "Connection: close\r\n");
    sprintf(buf + strlen(buf), "Content-length: %d\r\n", filesize);
    // 헤더의 끝을 알리는 빈 줄(\r\n)을 반드시 추가해야 함
    sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n", filetype);

    Rio_writen(fd, buf, strlen(buf));
    printf("Response headers : \n");
    printf("%s", buf);

    if(!flag) return;//HEAD메소드는 바디출력 하지않고 리턴

    //숙제 11.9
    //메모리 복사과정이 한번 더 일어나서 비효율적 
    srcfd = Open(filename,O_RDONLY, 0);     // 1. 파일 읽기 전용으로 열기

    srcp = (char *)Malloc(filesize);        // 2. 파일 크기만큼 메모리 할당
    Rio_readn(srcfd, srcp, filesize);       // 3. 파일 내용을 메모리로 모두 읽기 (복사)

    Close(srcfd);                           // 4. 파일 닫기
    Rio_writen(fd, srcp, filesize);         // 5. 메모리의 내용을 클라이언트에게 전송
    free(srcp);                             // 6. 할당한 메모리 해제


}

void get_filetype(char *filename, char *filetype){
    if (strstr(filename, ".html")) {
    strcpy(filetype, "text/html");
  } else if (strstr(filename, ".gif")) {
    strcpy(filetype, "image/gif");
  } else if (strstr(filename, ".png")) {
    strcpy(filetype, "image/png");
  } else if (strstr(filename, ".jpg")) {
    strcpy(filetype, "image/jpeg");
  } else if(strstr(filename, ".mp4")){//숙제 11.7 MP4비디오파일 처리
    strcpy(filetype, "video/mp4");
  } else {
    strcpy(filetype, "text/plain");
  }
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *method){
    char buf[MAXLINE];
    char *emptylist[] = {NULL};

    // 헤더 첫부분 클라로 보냄
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if(Fork() == 0){
        //자식프로세스는 앞으로 자신이 수행할 환경변수 설정
        setenv("QUERY_STRING", cgiargs, 1); //-> cgi인자로 받아온 두 숫자 환경변수에 저장
        setenv("REQUEST_METHOD", method, 1); //-> get인지 head인지를 환경변수에 저장

        //Dup2(A, B): "B가 A를 가리키도록 복제하라"는 의미
        Dup2(fd, STDOUT_FILENO);
        //터미널로 보이는 표준출력을 fd라는 소캣으로 복제하라?
        //이제 print하는 내용들은 터미널로 보여지지않고 클라이언트로 감

        Execve(filename, emptylist, environ);// 대충 cgi프로그램을 실행한다 정도만
    }
    Wait(NULL);
}