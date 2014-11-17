//author: jeff
//Email: zyfforlinux@163.com
//Site: www.51cto.xyz

/*
	Return code:
		-1 Usage failue
*/

#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <search.h>
#include "socket.h"

#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTIONS 2
#define METHOD_TRACE 3
#define PROGRAM_VERSION "1.0"
#define MAXSIZ 100
#define REQUEST_SIZE 2048
#define PTHREAD_MAX 65535 //可以创建线程的最大数目
#define MAXADDRLENG 255

int reload = 0; //是否启用Pargma: no-cache 不使用缓存
int force = 0; //是否不等待server返回数据
int method = METHOD_GET; //使用的http方法
int clients = 1; //发起多少个客户端连接，默认是1个
int benchtime = 60;	//默认持续发起请求的时间
char *proxyhost = NULL; //保存代理服务器的host部分
int proxyport=8080;	//保存代理服务器的端口
char hostname[MAXSIZ];
int hostport=80; //默认是80端口
char request_file[MAXSIZ]; //请求的文件
char request[REQUEST_SIZE]; //整个请求头
int timerexpired = 0;//time falg 
unsigned int failed = 0; //失败的客户端数目
unsigned int speed  = 0; //每分钟多少个页面
unsigned int bytes = 0; //接收到的字节大小
int start = 0; //开始/关闭标志
pthread_mutex_t lock;

struct args{
	char *host;
	int port;
};

static int is_proxy(char *proxy); //判断proxy server是否符合格式
static void deal_opt(int argc,char **argv);//处理选项
static void usage(void);
static int check_url(char *url);
static void print(char *rul);
static void build_request(char *url); //构建报文
void * bench(void *arg);

static int check_url(char *url)
{
//Example: correct url http:://www.baidu.com/请求的文件名(index.php index.html....)
	char *p = url;
	char *tmp = NULL;
	if(strlen(p) > 1500)
		return 0; //url too long

	if(strstr(p,"http://") == NULL)
		return 0; //不是以http://开头

	tmp = strrchr(p,'/');//返回最后一个'/'符号
	if(tmp == (p+6))
		return 0; //最后一个'/'要是紧跟host后面，否则错误

	if(tmp == (p+strlen(p)-1)) //说明host后面/符号的后面没有任何字符了,不符合要求，
		return 0;

	bzero(request_file,sizeof(request_file));
	strncpy(request_file,tmp,strlen(tmp)); //保存请求的文件

	//从url中解析处hostname,如果hostname含有端口号也要解析处端口号，否则默认是80
	p = p+7; //指向hostname的第一个字符
	bzero(hostname,sizeof(hostname));
	if(strchr(p,':') != NULL){
		//含有端口号
		tmp = strchr(p,':'); 
		hostport = atoi(tmp+1);  //获取端口号，转换成数字
	//	printf("port:%d\n",hostport);
	//	printf("len:%ld\n",tmp-p);
		strncpy(hostname,p,tmp-p);
	//	printf("hostname:%s\n",hostname);
		
	}else{
		//不含有端口号
		strncpy(hostname,p,tmp-p);
	//	printf("hostname:%s\n",hostname);
	}
	
	return 1;

}

static int is_proxy(char *proxy)
{
	char *p = NULL;
	proxyhost = proxy;
	if((p = strchr(proxyhost,':')) == NULL){
		printf("The proxy server is not correct format\n");
		return -1;
	}
	if(p == proxyhost)
	{
		
		printf("The proxy server is not correct host part\n");
		return 0;
	}
	if(p == (proxyhost+strlen(proxyhost)-1))//缺少端口号 目的是判断tmp的指向的地址
					  //是否是proxy字符串末尾的地址
	{
		printf("The proxy server is not correct port part\n");
		return 0;
	}
	*p = '\0'; //将proxy 分成两半，前面一部分是server 地址,后面一部分是端口号
	proxyport = atoi(p+1);
	if(proxyport > 65535 || proxyport < 0)
	{
		printf("The proxy server port not in th scope of 0~65535\n");
		return 0;
	}	
		return 1;	
}
static void usage(void)
{
	fprintf(stderr,
	"webbench++ [option]...URL\n"
	" -f|--force			Don't wait for reply from server.\n"
	" -r|--reload			Send reload request -Pargma: no-cache.\n"
	" -t|--time <sec> 		Run benchmark for <sec> seconds.Default 30.\n"
	" -p|--proxy <server:port> 	Use proxy server fro request.\n"
	" -c|--clients <n> 		Run <n> HTTP clients at once.Default one\n"
	" --get 				Use GET request method.\n"
	" --head				Use HEAD request method.\n"
	" --options			Use OPTIONS request method.\n"
	" --trace			Use TRACE request method.\n"
	" -?|-h|--help			This information.\n"
	" -V|--version			Display program version.\n"
	);
	exit(-1);
}

//处理选项
static void deal_opt(int argc,char **argv)
{
//getopt提供了三个外部变量 optarg指向当前选项的参数
//optind 指定选项的索引，0索引是程序本身名字，optopt是当前选项,
//opterr 设置为0，可以避免getopt输出错误信息
//       extern char *optarg;
//       extern int optind, opterr, optopt;

	int opt = 0; //返回分析到的选项
	int options_index = 0; //返回选项在long_options中的索引
	static struct option long_options[] = {
		{"force",no_argument,NULL,'f'},
		{"reload",no_argument,NULL,'r'},
		{"time",required_argument,NULL,'t'},
		{"help",no_argument,NULL,'h'},
		{"get",no_argument,&method,METHOD_GET},
		{"head",no_argument,&method,METHOD_HEAD},
		{"options",no_argument,&method,METHOD_OPTIONS},
		{"trace",no_argument,&method,METHOD_TRACE},
		{"version",no_argument,NULL,'V'},
		{"proxy",required_argument,NULL,'p'},
		{"clients",required_argument,NULL,'c'},
		{NULL,0,NULL,0}
	};
	
	while((opt = getopt_long(argc,argv,"frVt:hp:c:?h",long_options,&options_index)) != EOF)	       
	{
		switch(opt)
		{
			case 0:break;
			case 'f':force = 1;break;
			case 'r':reload = 1;break;
			case 'V':printf("Webbench++ Version:%s\n",PROGRAM_VERSION),exit(0);
			case 'p':
				if(!is_proxy(optarg)){
					usage();			
				}
				else
					break;
			case 'c':
					clients = atoi(optarg);
					break;
			case 't':
					benchtime = atoi(optarg);
					break;
			case 'h':
				usage();
			case '?':
				usage();
		
		}
	}
	
	//判断最后一个参数是否是URL
	if((optind+1) != argc){
		printf("You may lack of URL\n");
		usage();
	}
	
	//检查url的合法性
	if(!check_url(argv[optind])){
		printf("URL format not correct Example: http://www.baidu.com/index.html \n");
		exit(-1);	
	}

}

static void build_request(char *url)
{
	//初始化request数组
	bzero(request,sizeof(request));
	
	//构建http协议头部
	switch(method)
	{
		case METHOD_GET:  strcpy(request,"GET");break;
		case METHOD_HEAD: strcpy(request,"HEAD");break;
		case METHOD_OPTIONS: strcpy(request,"OPYIONS");break;
		case METHOD_TRACE: strcpy(request,"TRACE");break;
	}
	strcat(request," "); //请求头后面是一个空格
	if(proxyhost != NULL) strcat(request,url); //如果有代理服务器则请求头是整个URL
	else{
		strcat(request,request_file); //否则请求头后面是请求的文件 例如 /index.php
	}
	//添加请求的协议
	strcat(request," HTTP/1.0");
	strcat(request,"\r\n"); //每条头部信息后面都是\r\n
	
	//构建Host
	strcat(request,"Host: ");
	strcat(request,hostname);
	strcat(request,"\r\n");

	//构建User-Agent头部 
	strcat(request,"User-Agent: WebBench "PROGRAM_VERSION"\r\n");
	
	//构建pragma: no-cache
	if(reload && proxyhost != NULL) //不适用代理的情况下，这个头部才有效
		strcat(request,"Pragma: no-cache\r\n");
	
	//不使用长连接
//	strcat(request,"Connection: close\r\n");
	
	//在整个请求头后面加上空行
	strcat(request,"\r\n");
}

static void print(char *url)
{
	printf("\nBenchmarking: ");
	switch(method)
	{
		case METHOD_GET:
		default:
			printf("GET");break;
		case METHOD_HEAD:
			printf("HEAD");break;
		case METHOD_OPTIONS:
			printf("OPTIONS");break;
		case METHOD_TRACE:
			printf("TRACE");break;
	}
	printf(" %s",url);
	printf(" default using HTTP/1.1");
	printf(".\n");
	printf("%d clients",clients);
	printf(", running %d sec",benchtime);
	if(force) printf(".early socket close");
	if(proxyhost != NULL) printf(", via proxy server %s:%d",proxyhost,proxyport);
	if(reload) printf(", forcing reload");
	printf(".\n");
}


void* bench(void *arg)
{
	int rlen;
	char buf[1500];
	int s,i;
	rlen = strlen(request);
	while(!start);
	nexttry:while(1)
	{
		if(timerexpired) //时间到了
		{
			if(failed > 0) //说明上一次连接的进程可能被打断了，所以不算是失败进程
			{
				failed--;
			}
			//等待
			pthread_exit((void*)0);
			
		}
		
		s = Socket(((struct args *)arg)->host,((struct args *)arg)->port);	
		if(s < 0) {
			failed++;
			continue;
		} //连接失败
		i = write(s,request,rlen);
		if(i < 0){failed++;continue;}
		if(force == 0)
		{
			while(1)
			{
				if(timerexpired)break;
				i = read(s,buf,1500);
				if(i < 0){
					failed++;
					close(s);
					goto nexttry;
				}
				else
					if(i == 0){
						break;
					}
					else{
						bytes += i;
					}
					
			}
		}
		if(close(s)){failed++;continue;}
		speed++; //单位是page/mintue
	}	

	pthread_exit((void*)0);
}


void timeover(int sig)
{
	timerexpired = 1;
}

void printdata()
{
	  printf("\nSpeed=%d pages/min, %d bytes/sec.\nRequests: %d susceed, %d failed.\n",
                  (int)((speed+failed)/(benchtime/60.0f)),
                  (int)(bytes/(float)benchtime),
                  speed,
                  failed);
}


int main(int argc,char **argv)
{
	int i=0;
	struct args arg;
	pthread_attr_t attr;
	struct sigaction action;
	struct hostent *hp;
	char buf[MAXADDRLENG];
	pthread_t p[PTHREAD_MAX];
	//p = (pthread_t*)malloc(clients*sizeof(pthread_t));
	//初始化mutex
	pthread_mutex_init(&lock,NULL);
	//getopt处理选项
	deal_opt(argc,argv);
	
	fprintf(stderr,"Webbench++,Simple Web Benchmark "PROGRAM_VERSION"\n"
	"Copyright (c) Zyf,GPL Open Source Software.\n");
	
	//根据选项构造报文
	build_request(argv[argc-1]);
	//打印提示信息
	print(argv[argc-1]); //传入url
	
	//测试目标web服务器是否可以连接
	if(proxyhost == NULL){
		hp = gethostbyname(hostname);
		if(hp == NULL){
			printf("reslove hostname error\n");
			return -4;
		}
		i = Socket(hp->h_addr,hostport);
	}
	else{
		hp = gethostbyname(proxyhost);
		if(hp == NULL){
			printf("reslove proxyhost error\n");
			return -4;
		}
		printf("%s\n",hp->h_addr);
		i = Socket(hp->h_addr,proxyport);
	}

	if(i < 0){
		fprintf(stderr,"\nConnect to server failed,Aborting benchmark.\n");
		exit(-3);
	}
	close(i); //关闭sockkfd,启动测试作用
	
	//设置参数
	if(proxyhost == NULL){
		arg.host = hp->h_addr;
		arg.port = hostport;
	}else {
		arg.host = hp->h_addr;
		arg.port = proxyport;
	}
	bzero(p,sizeof(p));
	//创建多线程
	pthread_attr_init(&attr);
	//注册信号处理函数
	action.sa_handler = timeover;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	for(i = 0;i < clients;i++)
	{
		if(pthread_create(&p[i],NULL,bench,(void *)&arg))
			perror("client create failue:");
		
	}
	if(sigaction(SIGALRM,&action,NULL) < 0 )
		perror("sigaction:"),exit(-3);
	alarm(benchtime);
	start = 1;
	while(!timerexpired);
		printdata();
	pthread_attr_destroy(&attr);
	return 0;
}	

