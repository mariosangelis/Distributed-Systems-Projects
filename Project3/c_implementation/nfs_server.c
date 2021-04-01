#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "struct_lib.h"
#include <signal.h>
#include "mynfs_server_list.h"
#define GARBAGE_COLLECT_TIME 20
#define FILE_NOT_FOUND -12
#define MAX_DATA_SIZE 1024
long int current_time;
void alarm_handler(int sig){
	
	long int next_retr_time,min_active_time;
	//printf("Garbage collect time\n");
	garbage_collect(time(NULL)-current_time);
	//Find the minimum active time to set the alarm
	min_active_time=find_next_retr_time();
	if(min_active_time==-1){
		//List is empty
		printf("Set alarm to=%d\n",GARBAGE_COLLECT_TIME);
		alarm(GARBAGE_COLLECT_TIME);
	}
	else{
		//Set alarm to next retransmission time
		next_retr_time=min_active_time-((time(NULL)-current_time)-GARBAGE_COLLECT_TIME);
		printf("Set alarm to=%ld\n",next_retr_time);
		alarm(next_retr_time);
	}
	//Print server's list after garbage collect
	print_list();
	
}
int main(int args,char *argv[]){
	
	if(args<3){
		fprintf(stderr,"Wrong number of arguments\n");
		exit(EXIT_FAILURE);
	}
	int server_socket,fromlen=sizeof(struct sockaddr_in),fd,fd_client,tmod,time_counter=1,fileSize;
	struct nfs_message msg;
	struct stat finfo;
	struct sockaddr_in nfs_server,nfs_client;
	struct nfs_reply server_reply;
	struct sigaction act_alarm={{0}};
	
	act_alarm.sa_handler=alarm_handler;
	act_alarm.sa_flags=SA_RESTART;
	sigaction(SIGALRM,&act_alarm,NULL);
	//Create a socket for receiving messages
	server_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if(server_socket < 0){
		fprintf(stderr,"Opening datagram socket error.\n");
		exit(1);
	}
	else{printf("Opening datagram socket....OK.\n");}
	nfs_server.sin_family = AF_INET;
	nfs_server.sin_port = htons(atoi(argv[2]));
	inet_aton(argv[1],&nfs_server.sin_addr);
	
	if(bind(server_socket,(struct sockaddr*)&nfs_server, sizeof(nfs_server))){
		fprintf(stderr,"Binding datagram socket error.\n");
		exit(1);
	}
	else{printf("Binding datagram socket...OK.\n");}
	//Initialize nfs_server list
	nfs_server_list_init();
	
	alarm(GARBAGE_COLLECT_TIME);
	current_time=time(NULL);
	while(1){
		if(recvfrom(server_socket,&msg,sizeof(struct nfs_message),0,(struct sockaddr *)&nfs_client,(socklen_t *)&fromlen)<0){
			printf("Problem with recvfrom\n");
			exit(EXIT_FAILURE);
		}
		msg.type=ntohl(msg.type);
		msg.flags=ntohl(msg.flags);
		msg.fd_client=ntohl(msg.fd_client);
		msg.pos=ntohl(msg.pos);
		msg.tmod=ntohl(msg.tmod);
		msg.nofbytes=ntohl(msg.nofbytes);
		
		if(msg.type==OPEN){
			memset(&server_reply,0,sizeof(struct nfs_reply));
			//Create a file descriptor and return it to the client
			printf("File name=%s,flags=%d\n",msg.payload,msg.flags);
			fd=open(msg.payload,msg.flags,S_IRWXU);
			fstat(fd,&finfo);
			fileSize =finfo.st_size;
			printf("fileSize=%d\n",fileSize);
			if(fd==-1){
				fprintf(stderr,"Error in open\n");
				
				server_reply.fd_client=htonl(-1);
				if(sendto(server_socket,&server_reply,sizeof(struct nfs_reply),0,(struct sockaddr*)&nfs_client,fromlen) < 0){
					fprintf(stderr,"Sending reply message error\n");
					exit(EXIT_FAILURE);
				}
				continue;
			}
			srand(time(NULL)+time_counter);
			time_counter++;
			fd_client=rand();
			nfs_server_list_add(msg.payload,fd,fd_client,time(NULL)-current_time);
			
			server_reply.unique_id=msg.unique_id;
			server_reply.size=htonl(fileSize);
			server_reply.fd_client=htonl(fd_client);
			server_reply.tmod=htonl(0);
			printf("File descriptor returned to client is %d\n",fd_client);
			//Return file's mod time and the client's file descriptor to the client
			if(sendto(server_socket,&server_reply,sizeof(struct nfs_reply),0,(struct sockaddr*)&nfs_client,fromlen) < 0){
				fprintf(stderr,"Sending reply message error\n");
				exit(EXIT_FAILURE);
			}
		}
		else if(msg.type==READ){
			int server_fd,nofbytes=0,prev_nof_bytes=0,read_failed=0;
			memset(&server_reply,0,sizeof(struct nfs_reply));
			//Find the operating_system file descriptor for this file.Also,find the mod time for this file
			nfs_server_list_search(msg.fd_client,&server_fd,&tmod);
			if(server_fd!=-1){
				server_reply.type=htonl(-100);
				fstat(server_fd,&finfo);
				fileSize =finfo.st_size;
				//Read a page(MAX_DATA_SIZE bytes long) from the file
				lseek(server_fd,0,SEEK_SET);
				lseek(server_fd,(msg.pos/MAX_DATA_SIZE)*MAX_DATA_SIZE,SEEK_SET);
				
				if(msg.pos==fileSize){nofbytes=0;}
				else{
					while(nofbytes<MAX_DATA_SIZE){
						prev_nof_bytes=nofbytes;
						nofbytes+=read(server_fd,server_reply.payload+nofbytes,MAX_DATA_SIZE-nofbytes);
						if(nofbytes<0){
							fprintf(stderr,"Server read error\n");
							server_reply.unique_id=msg.unique_id;
							server_reply.nofbytes=htonl(-1);
							if(sendto(server_socket,&server_reply,sizeof(struct nfs_reply),0,(struct sockaddr*)&nfs_client,fromlen) < 0){
								fprintf(stderr,"Sending reply message error\n");
								exit(EXIT_FAILURE);
							}
							read_failed=1;
							break;
						}
						if(prev_nof_bytes==nofbytes){break;}
					}
					if(read_failed==1){continue;}
				}
				update_active_time(server_fd,time(NULL)-current_time);
			}
			else{server_reply.type=htonl(FILE_NOT_FOUND);}
			server_reply.size=htonl(fileSize);
			server_reply.unique_id=msg.unique_id;
			server_reply.nofbytes=htonl(nofbytes);
			server_reply.tmod=htonl(tmod);
			
			//Return data,file's mod time and the number of read bytes to the client
			if(sendto(server_socket,&server_reply,sizeof(struct nfs_reply),0,(struct sockaddr*)&nfs_client,fromlen) < 0){
				fprintf(stderr,"Sending reply message error\n");
				exit(EXIT_FAILURE);
			}
		}
		else if(msg.type==WRITE){
			int server_fd,nofbytes=0,write_failed=0;
			memset(&server_reply,0,sizeof(struct nfs_reply));
			//Find the operating_system file descriptor for this file.Also,find the mod time for this file
			nfs_server_list_search(msg.fd_client,&server_fd,&tmod);
			if(server_fd!=-1){
				server_reply.type=htonl(-100);
				lseek(server_fd,0,SEEK_SET);
				lseek(server_fd,msg.pos,SEEK_SET);
				//Write bytes to the file
				while(nofbytes<msg.nofbytes){
					nofbytes+=write(server_fd,msg.payload+nofbytes,msg.nofbytes-nofbytes);
					if(nofbytes<0){
						fprintf(stderr,"Server write error\n");
						server_reply.unique_id=msg.unique_id;
						server_reply.nofbytes=htonl(-1);
						if(sendto(server_socket,&server_reply,sizeof(struct nfs_reply),0,(struct sockaddr*)&nfs_client,fromlen) < 0){
							fprintf(stderr,"Sending reply message error\n");
							exit(EXIT_FAILURE);
						}
						write_failed=1;
						break;
					}
				}
				if(write_failed==1){continue;}
				fstat(server_fd,&finfo);
				fileSize =finfo.st_size;
				tmod=update_tmod(msg.fd_client);
				update_active_time(server_fd,time(NULL)-current_time);
			}
			else{server_reply.type=htonl(FILE_NOT_FOUND);}
			server_reply.size=htonl(fileSize);
			server_reply.unique_id=msg.unique_id;
			server_reply.tmod=htonl(tmod);
			server_reply.nofbytes=htonl(nofbytes);
			//Return file's mod time and the number of written bytes to the client
			if(sendto(server_socket,&server_reply,sizeof(struct nfs_reply),0,(struct sockaddr*)&nfs_client,fromlen) < 0){
				fprintf(stderr,"Sending reply message error\n");
				exit(EXIT_FAILURE);
			}
		}
	}
	return(0);
}
