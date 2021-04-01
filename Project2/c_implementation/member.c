#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "middleware.h"
#define MAX_MESSAGE_LENGTH 100
void *receive_messages();
int main(int args,char *argv[]){
	if(args<6) {
		fprintf(stderr,"Wrong number of arguments.Second argument should be group name.Third argument should be manager ip address.Fourth argument should be udp multicast port.Fifth argument should be ack port.Sixth argument must be member's ip address\n");
		exit(EXIT_FAILURE);
	}
	int group_sock_fd=-1,myid,iret,i;
	pthread_t thread;
	FILE* fp;
	char message[MAX_MESSAGE_LENGTH];
	
	while(group_sock_fd==-1){
		srand(getpid());
		myid=rand();
		group_sock_fd=grp_join(argv[1],argv[2],atoi(argv[3]),myid,atoi(argv[4]),argv[5]);
	}
	//This file includes messages
	fp = fopen("message.txt","r");
	if(fp==NULL){
		fprintf(stderr,"Error with fopen\n");
		exit(EXIT_FAILURE);
	}
	iret = pthread_create(&thread, NULL,receive_messages,NULL); 
	if(iret){
		fprintf(stderr,"Error - pthread_create() return code: %d\n",iret);
		exit(EXIT_FAILURE);
	}
	//Send a lot of messages to the multicast group
	//sleep(10);
	while(1){
        for(i=0;i<4000;i++){
            if (fgets(message,MAX_MESSAGE_LENGTH,fp) == NULL){          
                break;
            }
            if(strcmp(message,"Leave\n")==0){
                grp_leave(argv[1],argv[2],atoi(argv[3]),myid,atoi(argv[4]),argv[5]);
                break;
            }
            else{
                printf("Send message %s",message);
                grp_send(group_sock_fd,message,MAX_MESSAGE_LENGTH,myid);
            }
            bzero(message,MAX_MESSAGE_LENGTH);
        }
        while(1){}
	}
	printf("EXITING\n");
	return(0);
}
void *receive_messages(){
	
	char message[MAX_MESSAGE_LENGTH],temp[MAX_MESSAGE_LENGTH];
	int len;
	bzero(temp,MAX_MESSAGE_LENGTH);
	while(1){
		grp_recv(1,MSG_TYPE,message,&len,0);
		if(strcmp(message,temp)!=0){printf(ANSI_COLOR_RED"Message is %s"ANSI_COLOR_RESET,message);}
		printf(""ANSI_COLOR_RESET);
		bzero(message,MAX_MESSAGE_LENGTH);
	}
	return NULL;
}
