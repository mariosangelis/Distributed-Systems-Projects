#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "middleware.h"
struct server_prime_reply{     //local struct,not for middleware
	long int number;
	int flag;
};
int primetest(long int v);
int main(int argc, char *argv[]){
	struct final_struct server_reply_message;
	struct server_prime_reply my_reply;
	struct buffer_struct server_buf;
	int  len,ret_Request,primetest_ret,i;
	char buffer[1024];
	head=NULL;
	
	if(argc!=5) {
		fprintf(stderr,"Wrong number of arguments.Second argument should be service id.Third argument should be udp multicast address.Fourth argument should be udp multicast port.Fifth argument should be ip address\n");
		exit(EXIT_FAILURE);
	}
	server_buf.port=atoi(argv[3]);
	strcpy(server_buf.multicast_address,argv[2]);
    strcpy(server_buf.my_address,argv[4]);
	server_buf.svcid=atoi(argv[1]);
	memcpy(buffer,&server_buf,sizeof(struct buffer_struct));
	registerr((void *)buffer);
	/*In every 1 second,server tries to get a request.If there is a request,server executes prime_test for this request,else server sleeps for 1 second again*/
	while(1){
		for(i=0;i<20;i++){
			sleep(1);
			ret_Request=getRequest(atoi(argv[1]),&server_reply_message,&len);
			if(ret_Request!=-1){
				printf(ANSI_COLOR_GREEN"Server executes primetest,for number %s"ANSI_COLOR_RESET,server_reply_message.payload);
				primetest_ret=primetest(atol(server_reply_message.payload));
				my_reply.flag=primetest_ret;
				my_reply.number=atol(server_reply_message.payload);
				memcpy(server_reply_message.payload,&my_reply,1024);
				server_reply_message.reqid=ret_Request;
				server_reply_message.client_unique_id=server_reply_message.client_unique_id;
				sendReply(ret_Request,(void *)&server_reply_message,sizeof(struct final_struct));
			}
			else{printf("No requests found\n");}
		}
		//unregister(atoi(argv[1]));
		// sleep(5);
		//registerr((void *)buffer);
	}
	return 0;
}
int primetest(long int v){
	int k,flag=0;
	for(k=2;k<=v/2;++k){
		if(v%k==0){
			flag=1;
			break;
		}
	}
	/*1 is neither a prime nor a composite number*/
	if(v==1){return(2);}
	else{
		/*If flag==0,the number is a prime number,else it is not a prime number*/
		if(flag==0){return(0);}
		else{return(1);}
	}
	return 0;
}
