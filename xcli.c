#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define IPSTR "127.0.0.1"
#define PORT 80
#define BUFSIZE 4096
int send_data(pid_t dev_id)
{
        int sockfd, ret, i, h;
        struct sockaddr_in servaddr;
        char str1[4096], str2[4096], buf[BUFSIZE], *str;
        socklen_t len;
        fd_set   t_set1;
        

        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
                printf("创建网络连接失败,本线程即将终止---socket error!\n");
                exit(0);
        };

        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(PORT);
        if (inet_pton(AF_INET, IPSTR, &servaddr.sin_addr) <= 0 ){
                printf("创建网络连接失败,本线程即将终止--inet_pton error!\n");
                exit(0);
        };

        if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
                printf("连接到服务器失败,connect error!\n");
                exit(0);
        }
        printf("与远端建立了连接\n");

        //发送数据
        memset(str2, 0, 4096);
        sprintf(str2, "id%d=%d&", dev_id,dev_id);
	strcat(str2, "stat1=1&");
	strcat(str2, "stat2=2&");
	strcat(str2, "stat3=3&");
	strcat(str2, "para1=4&");
	strcat(str2, "para2=5&");
	strcat(str2, "para3=6&");
        str=(char *)malloc(128);
        len = strlen(str2);
        sprintf(str, "%d", len);

        memset(str1, 0, 4096);
        strcat(str1, "POST /iheard HTTP/1.1\n");
        strcat(str1, "Host: 127.0.0.1\n");
        strcat(str1, "Content-Type: text/html\n");
        strcat(str1, "Content-Length: ");
        strcat(str1, str);
        strcat(str1, "\n\n");

        strcat(str1, str2);
        strcat(str1, "\r\n\r\n");
        printf("%s\n",str1);

        ret = write(sockfd,str1,strlen(str1));
        if (ret < 0) {
                printf("发送失败！错误代码是%d，错误信息是'%s'\n",errno, strerror(errno));
                exit(0);
        }else{
                printf("消息发送成功，共发送了%d个字节！\n\n", ret);
        }

        FD_ZERO(&t_set1);
        FD_SET(sockfd, &t_set1);

        h= 0;
        printf("--------------->1");
        h= select(sockfd +1, &t_set1, NULL, NULL, NULL);
        printf("--------------->2\n");

                //if (h == 0) continue;
        if (h < 0) {
                close(sockfd);
                printf("在读取数据报文时SELECT检测到异常，该异常导致线程终止！\n");
                return -1;
        };

        if (h > 0){
                memset(buf, 0, BUFSIZE);
                i= read(sockfd, buf, BUFSIZE-1);
                if (i==0){
                       	close(sockfd);
        	  	printf("读取数据报文时发现远端关闭，该线程终止！\n");
                        return -1;
                }

                printf("%s\n", buf);
        }
        
        close(sockfd);
	return 0;	
}
int main(int argc, char **argv)
{
	int ncli;
	pid_t this, new,ppid;
	if (argc != 2) {
		printf("plese input right para\n");
		return 0;
	}
	ncli = atoi(argv[1]);
	while (--ncli > 0) {
		new = fork();
		if (new == 0)
			break;
		else if (new == -1)
			fprintf(stderr, "main:%s\n", strerror(errno));
	}
	this = getpid();
	send_data(this);
        return 0;
}
