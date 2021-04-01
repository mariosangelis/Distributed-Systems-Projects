#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h> 
#include "middleware.h"
#define PORT_START_NUM 5000
int main(int args,char *argv[]){
    
	if(args<2) {
		fprintf(stderr,"Wrong number of arguments.Second argument should be manager's ip address.Third argument should be manager's port.\n");
		exit(EXIT_FAILURE);
	}
	int port=PORT_START_NUM,member_exists_flag=0,connected_members=1,polling_fd,reamaining_members,leaving_members=0;
	struct sockaddr_in server,client;
	struct join_struct recv_member,polling_msg;
	struct group_node *search_group_ret;
	struct member_node *member_exists;
	struct manager_reply join_reply;
	int sock,connfd,len,sequencer=0,join_member_id;
	sock = socket(AF_INET, SOCK_STREAM,0);
	if(sock < 0){
		fprintf(stderr,"Opening datagram socket error.\n");
		exit(1);
	}
	else{printf("Opening datagram socket....OK.\n");}    
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(argv[2]));                   
	inet_aton(argv[1],&server.sin_addr);
	
	if(bind(sock, (struct sockaddr*)&server, sizeof(server))<0){
		fprintf(stderr,"Binding datagram socket error.\n");
		exit(1);
	}
	if(listen(sock,5)<0){
		fprintf(stderr,"Listen() error.\n");
		exit(1);
	}
	len=sizeof(struct sockaddr_in);
	//Initialize group list
	group_list_init();
	while(1){
		//Accept a join request from a client
		connfd = accept(sock,(struct sockaddr*)&client,(socklen_t *)&len); 
		if (connfd < 0) { 
			printf("server acccept failed...\n"); 
			exit(0); 
		} 
		else{printf("server acccept the client...\n");}
		//Receive a join request from a member
		recv(connfd,&recv_member,sizeof(struct join_struct),0);
		if(ntohl(recv_member.type)==JOIN_TYPE){
			search_group_ret=group_list_search(recv_member.group_name);
			if(search_group_ret!=NULL){                          
				//Group exists
				member_exists=group_list_id_search(recv_member.member_id,search_group_ret);
				if(member_exists==NULL){
					//Add the member in the group
					member_list_add(recv_member.member_id,search_group_ret);
                    join_member_id=recv_member.member_id;
					search_group_ret->members++;
				}
				else{
					//A member with the same id exists
					member_exists_flag=1;
				}
			}
			else{                                                         
				//Create team and add the member in the group
				search_group_ret=group_list_add(recv_member.group_name,port);
				sequencer=1;
				port ++;
				member_list_add(recv_member.member_id,search_group_ret);
				search_group_ret->members++;
			}
			print_group_list();
			if(member_exists_flag==0){                                          
				//Inform all members for the arrival of a new member by sending an update message in the multicast group
				if(search_group_ret->members>1){
					printf("Receiving polling messages from other members\n");
					while(connected_members<search_group_ret->members){
						polling_fd = accept(sock,(struct sockaddr*)&client,(socklen_t *)&len); 
						if (polling_fd < 0) { 
							printf("server acccept failed...\n"); 
							exit(0); 
						}
						recv(polling_fd,&recv_member,sizeof(struct join_struct),0);
						if(ntohl(recv_member.type)==POLLING_TYPE){
							printf("Got a polling message\n");
							struct send_message msg;
							msg.type=htonl(UPDATE_TYPE);
							msg.sequence_number=htonl(search_group_ret->members);
							msg.sender_pid=htonl(join_member_id);
							strcpy(msg.payload,"A new member was added in the group");
							
							if(send(polling_fd,&msg,sizeof(struct send_message),0) < 0){fprintf(stderr,"Sending update message error\n");}
							else{printf("Sending update message...OK\n");}
							connected_members++;
						}
						else{
							struct manager_reply msg;
							msg.type=htonl(NACK);
							
							if(send(polling_fd,&msg,sizeof(struct manager_reply),0) < 0){fprintf(stderr,"Sending update message error\n");}
							else{printf("Sending nack message...OK\n");}
						}
						
					}
					connected_members=1;
				}
				memset((struct join_struct *)&recv_member,0,sizeof(struct join_struct));
				//Inform the member for the sequencer,for the number of members in the group and for the multicast port which will be opened for sending messages to other members
				join_reply.port=htonl(search_group_ret->port);
				join_reply.members=htonl(search_group_ret->members);
				join_reply.is_sequencer=sequencer;
				sequencer=0;
				if(send(connfd,&join_reply,sizeof(struct manager_reply),0)<0){
					printf("Unable to send to TCP socket...\n"); 
					exit(0); 
				}
			}
			else{                                                                        //member exists,return error
				join_reply.port=htonl(-1);
				if(send(connfd,&join_reply,sizeof(struct manager_reply),0)<0){
					printf("Unable to send to TCP socket...\n"); 
					exit(0); 
				}
				member_exists_flag=0;
			}
			close(connfd);
		}
		else if(ntohl(recv_member.type)==LEAVE_TYPE){
			
			reamaining_members=member_list_delete(recv_member.member_id,recv_member.group_name);
			print_group_list();
			//Inform all members for a new member exit by sending an update message in the multicast group
			if(reamaining_members>=1){
				printf("Receiving polling messages from other members\n");
				while(leaving_members<reamaining_members){
					polling_fd = accept(sock,(struct sockaddr*)&client,(socklen_t *)&len); 
					if (polling_fd < 0) { 
						printf("server acccept failed...\n"); 
						exit(0); 
					}
					recv(polling_fd,&polling_msg,sizeof(struct join_struct),0);
					if(ntohl(polling_msg.type)==POLLING_TYPE && recv_member.member_id!=ntohl(polling_msg.member_id)){
						printf("Got a polling message\n");
						struct send_message msg;
						
						if(leaving_members==reamaining_members-1 && ntohl(recv_member.sequencer)==1){
							printf("Changing sequencer\n");
							msg.sequencer_sender_id=htonl(1);
						}
						else{
							msg.sequencer_sender_id=htonl(0);
						}
						msg.type=htonl(UPDATE_TYPE);
						msg.sequence_number=htonl(reamaining_members);
						msg.sender_pid=htonl(recv_member.member_id);
						strcpy(msg.payload,"A new member was left the group");
						
						if(send(polling_fd,&msg,sizeof(struct send_message),0) < 0){fprintf(stderr,"Sending update message error\n");}
						else{printf("Sending update message...OK\n");}
						leaving_members++;
					}
					else if(recv_member.member_id==ntohl(polling_msg.member_id)){}
					else{
						struct manager_reply msg;
						msg.type=htonl(NACK);
						
						if(send(polling_fd,&msg,sizeof(struct manager_reply),0) < 0){fprintf(stderr,"Sending update message error\n");}
						else{printf("Sending nack message...OK\n");}
					}
					
				}
				leaving_members=0;
			}
			else{
				group_delete(recv_member.group_name);
			}
			memset((struct join_struct *)&recv_member,0,sizeof(struct join_struct));
			//Inform the member for the sequencer,for the number of members in the group and for the multicast port which will be opened for sending messages to other members
			join_reply.port=htonl(0);
			join_reply.members=htonl(0);
			if(send(connfd,&join_reply,sizeof(struct manager_reply),0)<0){
				printf("Unable to send to TCP socket...\n"); 
				exit(0); 
			}
			close(connfd);
		}
		else{
			printf("Received polling message\n");
			struct send_message msg;
			msg.type=htonl(UPDATE_TYPE);
			msg.sequence_number=htonl(0);
			msg.sender_pid=htonl(recv_member.member_id);
			strcpy(msg.payload,"No updates found");
			
			if(send(connfd,&msg,sizeof(struct send_message),0) < 0){fprintf(stderr,"Sending update message error\n");}
			else{printf("Sending update message...OK\n");}
			close(connfd);
		}
	}
}
