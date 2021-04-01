#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "middleware.h"
#include "request_list.h"
void *reply_receive(void *arg);
/*Reply_struct includes a prime_num and information about primetest.This struct is returned to the client*/
struct reply_struct{  /*This is a local struct,not for middleware*/
	long int number;
	int flag;
};
int main(int args,char *argv[]) {
	char prime_num[1024],databuf[1024];
	int svcid,i,ret,temp_reqid,len,reqid;
	reply_head=NULL;
	RequestNode *head_req=NULL;
	FILE* fp;
	struct reply_struct client_reply_message;
	struct buffer_struct client_buffer_struct;
	
	if(args!=5) {
		fprintf(stderr,"Wrong number of arguments.Second argument should be service id.Third argument should be udp multicast address.Fourth argument should be udp multicast port,Fifth argument should be ip address\n");
		exit(EXIT_FAILURE);
	}
	fp = fopen("primes.txt", "r");
	if(fp==NULL){
		fprintf(stderr,"Error with fopen\n");
		exit(EXIT_FAILURE);
	}
	svcid=atoi(argv[1]);
	head_req=init_req_list(head_req);
	//sleep(10);
	while(1){
		for(i=0;i<5;i++){
			printf("Reading a number from file...\n");
			if (fgets(prime_num,100,fp) == NULL){              /*Reading numbers from a file,if EOF->break*/
				return(0);
			}
			bzero(databuf,1024);
			strncpy(client_buffer_struct.multicast_address,argv[2],64);
			strncpy(client_buffer_struct.my_address,argv[4],64);
			client_buffer_struct.port=atoi(argv[3]);
			strncpy(client_buffer_struct.payload,prime_num,1024);
			memcpy(databuf,&client_buffer_struct,sizeof(struct buffer_struct));
			len=sizeof(struct buffer_struct);
			temp_reqid=sendRequest(svcid,(void *)databuf,len);
			add_in_request_list(head_req,temp_reqid);
		}
		sleep(2);
		for(i=0;i<get_reqlist_length(head_req);i++){
			reqid=take_request(head_req,i);
			ret=getReply(reqid,&client_reply_message,0,1);
			if(ret==-1){printf(ANSI_COLOR_GREEN"No answer from getReply\n"ANSI_COLOR_RESET);}
			else{
				if(client_reply_message.flag==0){printf(ANSI_COLOR_GREEN"%ld is a prime number\n"ANSI_COLOR_RESET,client_reply_message.number);}
				else if(client_reply_message.flag==1){printf(ANSI_COLOR_GREEN"%ld is not a prime number\n"ANSI_COLOR_RESET,client_reply_message.number);}
				else{printf(ANSI_COLOR_GREEN"1 is neither a prime nor a composite number\n"ANSI_COLOR_RESET);}
				set_delete_flag(head_req,reqid);
			}
		}
		delete_request(head_req);
	}
	return(0);
}
