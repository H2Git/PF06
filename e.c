#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define MAX_CHAT 1000 // max chatting length
#define MAX_NAME 24	  // max nickname length

int ConnectServer(int AF, char *serv_ip, char *serv_port); // create clnt_sock, serv_sock & connnect, return serv_sock
void QuitOnError(char *msg);							   // error func

int main(int argc, char *argv[])
{
	char buf_msg[MAX_CHAT];			   // each chatting message buffer
	char buf_all[MAX_CHAT + MAX_NAME]; // total string for send to serv
	int select_fds;					   // sockets for select maxfd
	int clnt_sock;
	fd_set read_fds;
	time_t c_tm;
	struct tm tm;

	char *exit_strings = "/EXIT /Exit /exit /q /Q "; // strings for exit, check one of them

	if (argc != 4) // guide on console
	{
		printf("잘못된 연결입니다.\n입력 >> %s Server_IP Server_Port Your_Nickname \n", argv[0]);
		exit(0);
	}

	if ((clnt_sock = (ConnectServer(AF_INET, argv[1], argv[2]))) < 0)
		QuitOnError("ConnectServer fail");
	else
		puts("Server Connected :) [White:System,Yellow:Echo,Green:User]\n");

	select_fds = clnt_sock + 1;
	FD_ZERO(&read_fds);

	for (;;)
	{
		FD_SET(0, &read_fds);
		FD_SET(clnt_sock, &read_fds);

		if (select(select_fds, &read_fds, NULL, NULL, NULL) < 0)
			QuitOnError("select() fail");

		if (FD_ISSET(0, &read_fds))
		{
			if (fgets(buf_msg, MAX_CHAT, stdin))
			{
				fprintf(stderr, "\033[1A");	   // Move cursor up 1 line
				fprintf(stderr, "\033[1;33m"); // change color of text to Yellow

				c_tm = time(NULL);
				tm = *localtime(&c_tm);
				buf_msg[strlen(buf_msg) - 1] = 0;

				sprintf(buf_all, "[%02d:%02d:%02d]%s>%s", tm.tm_hour, tm.tm_min, tm.tm_sec, argv[3], buf_msg);

				fprintf(stderr, "\033[1;0m"); //change color of text to Default(White)
				if (send(clnt_sock, buf_all, strlen(buf_all), 0) < 0)
					puts("Error : Write error on socket.");

				if (strstr(exit_strings, buf_msg) != NULL)
				{
					puts("Disconnected. cya ;)");
					close(clnt_sock);
					exit(0);
				}
			}
		}

		if (FD_ISSET(clnt_sock, &read_fds))
		{
			int nbyte;
			nbyte = recv(clnt_sock, buf_msg, MAX_CHAT, 0);
			fprintf(stderr, "\033[1;0G"); // move to column position
			//write(1, "\033[0G", 4);		// move to column

			if (nbyte > 0)
			{
				buf_msg[nbyte] = 0;
				printf("%s\n", buf_msg);
				fprintf(stderr, "\033[1;32m");	 // Green Text
				fprintf(stderr, "%s>", argv[3]); // printf nickname
			}
			else
			{
				printf("[%02d:%02d:%02d]", tm.tm_hour, tm.tm_min, tm.tm_sec);
				printf("서버 오류. 채팅을 종료합니다.\n");

				close(clnt_sock);

				return 0;
			}
		}

	} // end of for(;;)
}

void QuitOnError(char *msg) // error func
{
	perror(msg);
	exit(1);
}

int ConnectServer(int AF, char *serv_ip, char *serv_port)
{
	int clnt_sock;
	if ((clnt_sock = socket(AF, SOCK_STREAM, 0)) < 0)
	{
		perror("Client Socket Error");
		exit(1);
	}

	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_port = htons(atoi(serv_port));
	serv_addr.sin_family = AF;
	inet_pton(AF_INET, serv_ip, &serv_addr.sin_addr); // IPv6 inet_pton ok, inet_aton() X, inet_addr() X
	if (connect(clnt_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		return -1;

	return clnt_sock;
}
