#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/select.h>

#define MAX_CHAT 1024 // Clients' Send Size
#define MAX_SOCK 1024

char *exit_strings = "/EXIT /Exit /exit /Q /q";	// String for clnt quit, one of them 
char *confirm_echo = "Welcome Mafia Chat :) [connect confirm msg from serv]";

int num_user = 0;			// 채팅 참가자 수
int num_chat = 0;			// 지금까지 오간 대화의 수
int clnt_socks[MAX_SOCK];	// 채팅에 참가자 소켓번호 목록
char ip_list[MAX_SOCK][20];	// 접속한 ip목록

time_t c_tm;
struct tm tm;

void QuitOnError(char *msg);
int ListenClients(int host, int port, int backlog); // socket create & listen
void *ManageSystem();		// func for thread
void AddClient(int sock, struct sockaddr_in *new_sockaddr);
void RemoveClient(int sock_index);	// 채팅 탈퇴 처리 함수
int GetMaxSockNum(int listen_sock);		// 최대 소켓 번호 찾기

int main(int argc, char *argv[]) 
{
	struct sockaddr_in clntaddr;
	char buf[MAX_CHAT + 1]; //클라이언트에서 받은 메시지
	int i, j, nbyte, accp_sock;
	int select_fds;				// max + 1
	int lstn_sock;

	socklen_t addr_sz = sizeof(struct sockaddr_in);
	fd_set read_fds;	//읽기를 감지할 fd_set 구조체
	pthread_t a_thread;

	if (argc != 2) 
	{
		printf("사용법 :%s port\n", argv[0]);
		exit(0);
	}

	// ListenClients(host, port, backlog) 함수 호출
	lstn_sock = ListenClients(INADDR_ANY, atoi(argv[1]), 5);
	//스레드 생성
	pthread_create(&a_thread, NULL, ManageSystem, (void *)NULL);

	for(;;) 
	{
		FD_ZERO(&read_fds);
		FD_SET(lstn_sock, &read_fds);

		for (i = 0; i < num_user; i++)
			FD_SET(clnt_socks[i], &read_fds);

		select_fds = GetMaxSockNum(lstn_sock) + 1;	// select_fds 재 계산

		if (select(select_fds, &read_fds, NULL, NULL, NULL) < 0)
			QuitOnError("select fail");

		if (FD_ISSET(lstn_sock, &read_fds)) 
		{
			accp_sock = accept(lstn_sock, (struct sockaddr*)&clntaddr, &addr_sz);
			if (accp_sock == -1) 
				QuitOnError("accept fail");

			AddClient(accp_sock, &clntaddr);
			send(accp_sock, confirm_echo, strlen(confirm_echo), 0);

			c_tm = time(NULL);			//현재 시간을 받아옴
			tm = *localtime(&c_tm);
			write(1, "\033[0G", 4);		//커서의 X좌표를 0으로 이동
			printf("[%02d:%02d:%02d]", tm.tm_hour, tm.tm_min, tm.tm_sec);
			fprintf(stderr, "\033[33m");//글자색을 노란색으로 변경
			printf("채팅 사용자 1명 추가. 현재 참가자 수 = %d\n", num_user);
			fprintf(stderr, "\033[32m");//글자색을 녹색으로 변경
			fprintf(stderr, "server>"); //커서 출력
		}

		// 클라이언트가 보낸 메시지를 모든 클라이언트에게 방송
		for (i = 0; i < num_user; i++) 
		{
			if (FD_ISSET(clnt_socks[i], &read_fds)) 
			{
				num_chat++;				//총 대화 수 증가
				nbyte = recv(clnt_socks[i], buf, MAX_CHAT, 0);

				if (nbyte <= 0) // 강제, 오류 연결 종료시 클라이언트 해제
				{
					RemoveClient(i);
					continue;
				}
				buf[nbyte] = 0;

				if (strstr(buf, exit_strings) != NULL)  // 종료 문자를 클라이언트 종료
				{
					RemoveClient(i);
					continue;
				}

				// 모든 채팅 참가자에게 메시지 방송
				for (j = 0; j < num_user; j++)
					send(clnt_socks[j], buf, nbyte, 0);

				printf("\033[0G");		//커서의 X좌표를 0으로 이동
				fprintf(stderr, "\033[97m");//글자색을 흰색으로 변경
				printf("%s\n", buf);			//메시지 출력
				fprintf(stderr, "\033[32m");//글자색을 녹색으로 변경
				fprintf(stderr, "server>"); //커서 출력
			}
		}

	}  // end of for(;;)

	return 0;
}

void QuitOnError(char *msg) // error func
{ 
	perror(msg); 
	exit(1); 
}

void *ManageSystem() { //명령어를 처리할 스레드

	int i;

	printf("[System:/help, /num_user, /num_chat, /ip_list]\n");

	while (1) 
	{
		char bufmsg[MAX_CHAT + 1];

		fprintf(stderr, "\033[1;32m"); //글자색을 녹색으로 변경
		printf("server>"); //커서 출력
		fgets(bufmsg, MAX_CHAT, stdin); //명령어 입력

		if (!strcmp(bufmsg, "\n")) // 엔터 무시
			continue;
		else if (!strcmp(bufmsg, "/help\n"))
			printf("help, num_user, num_chat, ip_list\n");
		else if (!strcmp(bufmsg, "/num_user\n"))
			printf("현재 참가자 수 = %d\n", num_user);
		else if (!strcmp(bufmsg, "/num_chat\n"))
			printf("전체 대화의 수 = %d\n", num_chat);
		else if (!strcmp(bufmsg, "/ip_list\n"))
			for (i = 0; i < num_user; i++)
				printf("%s\n", ip_list[i]);
		else
			printf("해당 명령어가 없습니다. /help를 참조하세요.\n");
	}
}

int ListenClients(int host, int port, int backlog) {

	int serv_sock;
	if((serv_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket() fail");
		exit(1);
	}

	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = htonl(host);

	if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
	{
		perror("bind() fail");  
		exit(1);
	}

	listen(serv_sock, backlog);

	return serv_sock;
}

void AddClient(int sock, struct sockaddr_in *new_sockaddr) {

	char buf[20];

	inet_ntop(AF_INET, &new_sockaddr->sin_addr, buf, sizeof(buf));

	fprintf(stderr, "\033[1;0G");   // move to column position
	// write(1, "\033[0G", 4);
	fprintf(stderr, "\033[33m");	// printf Yellow text
	printf("new client: %s\n", buf);//ip출력

	clnt_socks[num_user] = sock; // 채팅 클라이언트 목록에 추가
	num_user++; //유저 수 증가

	strcpy(ip_list[num_user], buf);
}

void RemoveClient(int sock_index) 
{
	close(clnt_socks[sock_index]);

	if (sock_index != num_user - 1) 
	{ //저장된 리스트 재배열
		clnt_socks[sock_index] = clnt_socks[num_user - 1];
		strcpy(ip_list[sock_index], ip_list[num_user - 1]);
	}

	num_user--; //유저 수 감소

	c_tm = time(NULL);			//현재 시간을 받아옴
	tm = *localtime(&c_tm);

	fprintf(stderr, "\033[1;0G");   // move to column position
	//write(1, "\033[0G", 4);		//커서의 X좌표를 0으로 이동
	fprintf(stderr, "\033[33m");//글자색을 노란색으로 변경
	printf("[%02d:%02d:%02d]", tm.tm_hour, tm.tm_min, tm.tm_sec);
	printf("채팅 참가자 1명 탈퇴. 현재 참가자 수 = %d\n", num_user);
	fprintf(stderr, "\033[32m");//글자색을 녹색으로 변경
	fprintf(stderr, "server>"); //커서 출력
}

int GetMaxSockNum(int listen_sock) 
{
	// Minimum 소켓번호는 가정 먼저 생성된 lstn_sock
	int max = listen_sock;
	int i;
	for (i = 0; i < num_user; i++)
		if (clnt_socks[i] > max)
			max = clnt_socks[i];

	return max;
}

