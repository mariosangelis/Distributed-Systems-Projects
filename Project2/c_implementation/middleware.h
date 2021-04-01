/*Angelis Marios-Kasidakis Theodoros*/
/*AEM : 2406-2258*/
#include <pthread.h>
#include <signal.h>
#include "struct_lib.h"
#include "group_list.h"
#include "reliable_multicast_list.h"
#define MAX_MESSAGE_LENGTH 100
#define MSG_TYPE -23
#define SEQ_TYPE -24
#define ACK_MSG_TYPE -25
#define ACK_SEQ_TYPE -26
#define ACK_UPDATE_TYPE -27
#define UPDATE_TYPE -28
#define JOIN_TYPE -290
#define LEAVE_TYPE -291
#define POLLING_TYPE -300
#define NACK -301
#define MESSAGE -21
#define SEQUENCE -22
#define RETRANSMISSION_TIME 10
#define POLLING_TIME 20
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_GREEN   "\e[0;32m"

int seqno=0,sequencer_order=0,deliver_to_api_order=1,udp_sock,ack_sock,is_sequencer=0,first_retransmission=0,sender_pid_retransmit,sequence_retransmit,members=1,mtx,receive_sem,blocked_member=0,new_k,sender_k,global_myid,msg_counter=0;
char myip[100];
pthread_t poll_thread;
time_t current_time;
struct sockaddr_in member,ack_member,client,servaddr; 
struct ip_mreq group;
struct sigaction act_alarm={{0}};
struct in_addr out_addr;
int grp_join(char *name,char *addr,int port,int myid,int ack_port,char *my_address);
int grp_send(int gsock,void *message,int len,int sender_pid);
int grp_leave(char *name,char *addr,int port,int myid,int ack_port,char *my_address);
int grp_recv(int gsock,int type,void *msg,int *len,int block);
void resend(int gsock,struct send_message msg);
void *Receiver(void *arg);
void *ACK_Receiver();
void *polling_thread(void *arg);
void alarm_handler(int sig){
	
	long int next_retransmission_time,min=0;
	int ret,type=MESSAGE;
	struct send_message msg;
	while(1){
		mysem_down(mtx);
		garbage_collect(deliver_to_api_order,min_k_head);
		ret=check_acks(sender_pid_retransmit,sequence_retransmit,members,type);
		if(ret==MESSAGE_ACKS_MISSING){
			//Resending message from handler
			//Find sender_pid_retransmit.sequence_retransmit message in the message list and copy all of his message struct elements to the function definitions 
			resend_from_handler(sender_pid_retransmit,sequence_retransmit,&msg.sender_pid,&msg.sequence_number,msg.payload,&msg.delivery_number,&msg.sequencer_sender_id,1);
			msg.type=MSG_TYPE;
			resend(udp_sock,msg);
		}
		else if(ret==SEQUENCE_ACKS_MISSING){
			//Resending sequence message from handler
			//Find sender_pid_retransmit.sequence_retransmit message in the message list and copy all of his message struct elements to the function definitions 
			resend_from_handler(sender_pid_retransmit,sequence_retransmit,&msg.sender_pid,&msg.sequence_number,msg.payload,&msg.delivery_number,&msg.sequencer_sender_id,0);
			msg.type=SEQ_TYPE;
			resend(udp_sock,msg);
		}
		//Find minimum send time 
		min=find_min_start_time(&sender_pid_retransmit,&sequence_retransmit,&type);
		if(min==RESET_FIRST_FLAG){
			alarm(RETRANSMISSION_TIME);
			//printf("Set alarm to %d\n",RETRANSMISSION_TIME);
			printf("Counter=%d\n",msg_counter);
			first_retransmission=0;
			mysem_up(mtx);
			break;
		}
		//Set alarm to new retransmission time 
		next_retransmission_time=min-((time(NULL)-current_time)-RETRANSMISSION_TIME);
		if(next_retransmission_time>0){
			//printf("Set alarm to %ld\n",next_retransmission_time);
			alarm(next_retransmission_time);
			mysem_up(mtx);
			break;
		}
		mysem_up(mtx);
	}
}
int grp_join(char *name,char *addr,int port,int myid,int ack_port,char *my_address){
	
	int sock,iret,ack_iret,poll_iret; 
	pthread_t thread,ack_thread;
	struct manager_reply join_reply;
	struct args *passing_arg=(struct args *)malloc(sizeof(struct args));
	struct join_struct new_member;
	
	strcpy(myip,my_address);
	current_time=time(NULL);
	printf("current_time is %ld\n",time(NULL)-current_time);
	mtx=mysem_create(mtx,1);               
	receive_sem=mysem_create(receive_sem,0);
	
	strcpy(new_member.group_name,name);
	new_member.type=htonl(JOIN_TYPE);
	new_member.member_id=myid;
	global_myid=myid;
	printf("Member's id is %d\n",new_member.member_id);
	while(1){
		//Create a TCP socket to connect with group manager
		sock = socket(AF_INET, SOCK_STREAM, 0); 
		if (sock == -1) { 
			printf("TCP Socket creation failed\n"); 
			exit(1); 
		} 
		else{printf("TCP Socket successfully created\n"); }
		//Assign IP, PORT 
		servaddr.sin_family = AF_INET; 
		inet_aton(addr,&servaddr.sin_addr);
		servaddr.sin_port = htons(port); 
		//Connect member and group manager sockets 
		if(connect(sock,(struct sockaddr *)&servaddr, sizeof(servaddr)) != 0){ 
			printf("Connection with the manager failed\n"); 
			exit(1); 
		} 
		else{printf("Connected with the manager\n"); }
		
		send(sock,&new_member,sizeof(struct join_struct),0);
		recv(sock,&join_reply,sizeof(struct manager_reply),0);
		close(sock);
		if(ntohl(join_reply.type)!=NACK){break;}
		sleep(5);
	}
	if(ntohl(join_reply.port)!=-1){
		act_alarm.sa_handler=alarm_handler;
		act_alarm.sa_flags=SA_RESTART;
		sigaction(SIGALRM,&act_alarm,NULL);
		//Create a udp socket for sending messages 
		udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
		if(udp_sock < 0){
			fprintf(stderr,"Opening datagram socket error.\n");
			exit(1);
		}
		else{printf("Opening datagram socket....OK.\n");}
		u_int yes = 1;
		//Allow to other members to reuse the same port and the same ip address
		if(setsockopt(udp_sock, SOL_SOCKET, SO_REUSEPORT, (char*) &yes, sizeof(yes)) < 0){perror("Reusing PORT failed\n");}
		if(setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR, (char*) &yes, sizeof(yes)) < 0){perror("Reusing ADDR failed\n");}
		member.sin_family = AF_INET;
		member.sin_port = htons(ntohl(join_reply.port));
		member.sin_addr.s_addr = htonl(INADDR_ANY);
		
		if(bind(udp_sock, (struct sockaddr*)&member, sizeof(member))){
			fprintf(stderr,"Binding datagram socket error.\n");
			exit(1);
		}
		else{printf("Binding datagram socket...OK.\n");}
		//Become a member of the udp multicast group
		group.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
		if(setsockopt(udp_sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,&group, sizeof(group)) < 0){
			perror("Adding multicast group error\n");
			exit(1);
		}
		else{printf("Adding multicast group...OK.\n");}
		members=ntohl(join_reply.members);
		//Create a receiver thread to receive all messages from group
		reliable_multicast_list_init();
		min_k_list_init();
		min_k_list_add(myid);
		passing_arg->port=ntohl(join_reply.port);
		passing_arg->gsock=udp_sock;
		passing_arg->myid=myid;
		passing_arg->is_sequencer=join_reply.is_sequencer;
		is_sequencer=join_reply.is_sequencer;
		iret = pthread_create(&thread, NULL,Receiver,(void *)passing_arg); 
		if(iret){
			fprintf(stderr,"Error - pthread_create() return code: %d\n",iret);
			exit(EXIT_FAILURE);
		}
		client.sin_family = AF_INET;
		inet_aton("239.255.255.250",&client.sin_addr);
		client.sin_port = htons(ntohl(join_reply.port));
		
		//Create a udp socket for receiving ack messages
		ack_sock = socket(AF_INET, SOCK_DGRAM, 0);
		if(ack_sock < 0){
			fprintf(stderr,"Opening ack datagram socket error.\n");
			exit(1);
		}
		else{printf("Opening ack datagram socket....OK.\n");}
		ack_member.sin_family = AF_INET;
		ack_member.sin_port = htons(ack_port);
        inet_aton(myip,&ack_member.sin_addr);
		//ack_member.sin_addr.s_addr=htonl(INADDR_ANY);
		
		if(bind(ack_sock, (struct sockaddr*)&ack_member, sizeof(ack_member))){
			fprintf(stderr,"Binding ack datagram socket error.\n");
			exit(1);
		}
		else{printf("Binding ack datagram socket...OK.\n");}
		//printf("ack port is %d and ack address is %s\n",ntohs(ack_member.sin_port),inet_ntoa(ack_member.sin_addr));
		//Create a thread for receiving acks
		ack_iret = pthread_create(&ack_thread, NULL,ACK_Receiver,NULL); 
		if(ack_iret){
			fprintf(stderr,"Error - ack pthread_create() return code: %d\n",ack_iret);
			exit(EXIT_FAILURE);
		}
		
		poll_iret = pthread_create(&poll_thread, NULL,polling_thread,(void *)&servaddr); 
		if(poll_iret){
			fprintf(stderr,"Error - polling pthread_create() return code: %d\n",poll_iret);
			exit(EXIT_FAILURE);
		}
	}
	else{udp_sock=-1;}
	return(udp_sock);
}
int grp_leave(char *name,char *addr,int port,int myid,int ack_port,char *my_address){
		
	int sock;
	struct manager_reply leave_reply;
	struct join_struct new_member;
	
	//pthread_kill(poll_thread,SIGINT);
	strcpy(new_member.group_name,name);
	if(is_sequencer==1){
		new_member.sequencer=htonl(1);
	}
	new_member.type=htonl(LEAVE_TYPE);
	new_member.member_id=myid;
	printf("Member's id is %d\n",new_member.member_id);
	while(1){
		//Create a TCP socket to connect with group manager
		sock = socket(AF_INET, SOCK_STREAM, 0); 
		if (sock == -1) { 
			printf("TCP Socket creation failed\n"); 
			exit(1); 
		} 
		else{printf("TCP Socket successfully created\n"); }
		//Connect member and group manager sockets 
		if(connect(sock,(struct sockaddr *)&servaddr, sizeof(servaddr)) != 0){ 
			printf("Connection with the manager failed\n"); 
			exit(1); 
		} 
		else{printf("Connected with the manager\n"); }
		
		send(sock,&new_member,sizeof(struct join_struct),0);
		recv(sock,&leave_reply,sizeof(struct manager_reply),0);
		close(sock);
		if(ntohl(leave_reply.type)!=NACK){break;}
		sleep(5);
	}
	return(0);
}
int grp_send(int gsock,void *message,int len,int sender_pid){
	
	struct send_message msg;
	int reliable_list_ret;
	
	msg.type=htonl(MSG_TYPE);
	seqno++;
	msg.sequence_number=seqno;
	strcpy(msg.payload,message);
	msg.sender_pid=sender_pid;
	if(is_sequencer==1){
		//This member is the sequencer
		//Search in the message list.If the function reliable_multicast_list_search returns MESSAGE_DOES_NOT_EXIST,add the message in the list
		reliable_list_ret=reliable_multicast_list_search(msg.sender_pid,msg.sequence_number);
		if(reliable_list_ret==MESSAGE_DOES_NOT_EXIST){
			//Add the message in the message list
			reliable_multicast_list_add(msg.payload,msg.sequence_number,msg.sender_pid,0);
			sequencer_order++;
			copy_seqnum(msg.sender_pid,msg.sequence_number,sequencer_order,sender_pid);
			
			msg.sequencer_sender_id=ntohl(0);
			msg.sequence_number=htonl(seqno);
			msg.sender_pid=htonl(sender_pid);
			msg.ack_port=ack_member.sin_port;
            strcpy(msg.ack_addr,myip);
			//Send the message to the multicast group
			set_message_start_time(sender_pid,seqno,time(NULL)-current_time);
            msg_counter++;
			if(sendto(gsock,&msg,sizeof(struct send_message),0,(struct sockaddr*)&client,sizeof(client)) < 0){
				fprintf(stderr,"Sending message error\n");
			}
			//Produce a sequence number for this message and send it to all other members
			struct send_message sequencer_msg;
			sequencer_msg.type=htonl(SEQ_TYPE);
			sequencer_msg.sequencer_sender_id=htonl(sender_pid);
			sequencer_msg.sender_pid=msg.sender_pid;
			sequencer_msg.sequence_number=msg.sequence_number;
			sequencer_msg.delivery_number=htonl(sequencer_order);
			sequencer_msg.ack_port=ack_member.sin_port;
			strcpy(sequencer_msg.ack_addr,myip);
			bzero(sequencer_msg.payload,MAX_MESSAGE_LENGTH);
			set_sequence_start_time(sender_pid,seqno,time(NULL)-current_time);
			
			//Send the sequence number to the multicast group
            msg_counter++;
			if(sendto(gsock,&sequencer_msg,sizeof(struct send_message),0,(struct sockaddr*)&client,sizeof(client)) < 0){
				fprintf(stderr,"Sending sequence message error\n");
			}
			if(first_retransmission==0){
				//Set alarm to RETRANSMISSION_TIME only in the first message send
				alarm(RETRANSMISSION_TIME);
				sender_pid_retransmit=sender_pid;
				sequence_retransmit=seqno;
				first_retransmission=1;
			}
			if(blocked_member==1 && ntohl(sequencer_msg.delivery_number)==deliver_to_api_order){
				blocked_member=0;
				mysem_up(receive_sem);
			}
		}
	}
	else{
		//This member is not the sequencer
		//Add the message in the message list
		reliable_multicast_list_add(msg.payload,msg.sequence_number,msg.sender_pid,0);
		set_message_start_time(sender_pid,seqno,time(NULL)-current_time);
		msg.sequence_number=htonl(seqno);
		msg.sender_pid=htonl(sender_pid);
		msg.ack_port=ack_member.sin_port;
		strcpy(msg.ack_addr,myip);
		msg.sequencer_sender_id=ntohl(0);
		
        msg_counter++;
		//Send the message to the multicast group
		if(sendto(gsock,&msg,sizeof(struct send_message),0,(struct sockaddr*)&client,sizeof(client)) < 0){
			fprintf(stderr,"Sending message error\n");
		}
		if(first_retransmission==0){
			//Set alarm to RETRANSMISSION_TIME only in the first message send
			alarm(RETRANSMISSION_TIME);
			sender_pid_retransmit=sender_pid;
			sequence_retransmit=seqno;
			first_retransmission=1;
		}
	}
	return(0);
}
int grp_recv(int gsock,int type,void *msg,int *len,int block){
	int ret;
	ret=return_message_to_api(msg,deliver_to_api_order);
	if(ret==1){
		bzero(msg,MAX_MESSAGE_LENGTH);
		*len=strlen(msg);
		if(block==1){
			blocked_member=1;
			//mysem_up(mtx);
			//mysem_down(receive_sem);
			//mysem_down(mtx);
			return_message_to_api(msg,deliver_to_api_order);
			*len=strlen(msg);
			deliver_to_api_order++;
		}
	}
	else{
		*len=strlen(msg);
		deliver_to_api_order++;
		if(deliver_to_api_order==4000+2){
			printf("####################################################################RECEIVED ALL MESSAGES######################################################################\n");
            printf("Counter=%d\n",msg_counter);
		}
	}
	return(0);
}
void resend(int gsock,struct send_message msg){
	
	//Set message send time or sequence message send time
	if(msg.type==MSG_TYPE){set_message_start_time(msg.sender_pid,msg.sequence_number,time(NULL)-current_time);}
	else if(msg.type==SEQ_TYPE){set_sequence_start_time(msg.sender_pid,msg.sequence_number,time(NULL)-current_time);}
	
	//printf("Resending...\n");
	msg.type=htonl(msg.type);
	msg.sender_pid=htonl(msg.sender_pid);
	msg.sequence_number=htonl(msg.sequence_number);
	msg.delivery_number=htonl(msg.delivery_number);
	msg.ack_port=ack_member.sin_port;
	strcpy(msg.ack_addr,myip);
	msg.sequencer_sender_id=htonl(msg.sequencer_sender_id);
	//msg.ack_addr=ack_member.sin_addr;
	
	//Send message to the multicast group
	if(sendto(udp_sock,&msg,sizeof(struct send_message),0,(struct sockaddr*)&client,sizeof(client)) < 0){
		fprintf(stderr,"Resending message error\n");
	}
	if(first_retransmission==0){
		//Set alarm to RETRANSMISSION_TIME only in the first message send
		alarm(RETRANSMISSION_TIME);
		sender_pid_retransmit=ntohl(msg.sender_pid);
		sequence_retransmit=seqno;
		first_retransmission=1;
	}
}
void *Receiver(void *arg){
	
	int reliable_list_ret,fromlen,delivery_num,min_k_ret,sock;
	struct sockaddr_in ack,update_ack;
	struct args *args=(struct args *)arg;
	struct send_message msg;
	
	fromlen=sizeof(struct sockaddr_in);
	if(args->is_sequencer==1){printf(ANSI_COLOR_GREEN"This process is sequencer\n"ANSI_COLOR_RESET);}
	while(1){
		if(recvfrom(args->gsock,&msg,sizeof(struct send_message),0,(struct sockaddr *)&update_ack,(socklen_t *)&fromlen)<0){
			printf("Problem with recvfrom\n");
			return NULL;
		}
		mysem_down(mtx);
		msg.type=ntohl(msg.type);
		msg.sender_pid=ntohl(msg.sender_pid);
		msg.sequence_number=ntohl(msg.sequence_number);
		msg.delivery_number=ntohl(msg.delivery_number);
		msg.sequencer_sender_id=ntohl(msg.sequencer_sender_id);
		
		ack.sin_family = AF_INET;
		inet_aton(msg.ack_addr,&ack.sin_addr);
		ack.sin_port =msg.ack_port;
		
		min_k_list_add(msg.sender_pid);
		if(msg.type==MSG_TYPE){
			//Got a message
			//This member is the sequencer
			min_k_ret=update_min_k(msg.sender_pid,msg.sequence_number);
			if(min_k_ret==K_IS_BIGGER_NOT_UPDATE || min_k_ret==UPDATE_K){
				if(min_k_ret==UPDATE_K){
					new_k=update_min_k_from_request_list(msg.sender_pid,msg.sequence_number);
					sender_k=msg.sender_pid;
					copy_new_k(msg.sender_pid,new_k);
				}
				if(is_sequencer==1){
					//Search in the message list.If the function reliable_multicast_list_search returns MESSAGE_DOES_NOT_EXIST,add the message in the list
					reliable_list_ret=reliable_multicast_list_search(msg.sender_pid,msg.sequence_number);
					if(reliable_list_ret==MESSAGE_DOES_NOT_EXIST){
						reliable_multicast_list_add(msg.payload,msg.sequence_number,msg.sender_pid,args->myid);
						//Resend the message using reliable multicast group messaging technique
						if(msg.sender_pid!=args->myid){
							msg_counter++;
							resend(args->gsock,msg);
						}
						sequencer_order++;
						copy_seqnum(msg.sender_pid,msg.sequence_number,sequencer_order,args->myid);
						set_sequence_start_time(msg.sender_pid,msg.sequence_number,time(NULL)-current_time);
						//Produce a sequence number for this message and send it to all other members
						struct send_message sequencer_msg;
						sequencer_msg.sequencer_sender_id=htonl(args->myid);
						sequencer_msg.type=htonl(SEQ_TYPE);
						sequencer_msg.sender_pid=htonl(msg.sender_pid);
						sequencer_msg.sequence_number=htonl(msg.sequence_number);
						sequencer_msg.delivery_number=htonl(sequencer_order);
						sequencer_msg.ack_port=ack_member.sin_port;
                        strcpy(sequencer_msg.ack_addr,myip);
						//sequencer_msg.ack_addr=ack_member.sin_addr;
						bzero(sequencer_msg.payload,MAX_MESSAGE_LENGTH);
						//Send the sequence number to the multicast group
                        msg_counter++;
						if(sendto(args->gsock,&sequencer_msg,sizeof(struct send_message),0,(struct sockaddr*)&client,sizeof(client)) < 0){
							fprintf(stderr,"Sending sequence message error\n");
						}
						if(blocked_member==1 && ntohl(sequencer_msg.delivery_number)==deliver_to_api_order){
							blocked_member=0;
							mysem_up(receive_sem);
						}
					}
				}
				else{
					//This member is not the sequencer
					reliable_list_ret=reliable_multicast_list_search(msg.sender_pid,msg.sequence_number);
					//Message not exists in the message list,so add it
					if(reliable_list_ret==MESSAGE_DOES_NOT_EXIST){
						reliable_multicast_list_add(msg.payload,msg.sequence_number,msg.sender_pid,msg.sequencer_sender_id);
						//Resend the message using reliable multicast group messaging technique
						if(msg.sender_pid!=args->myid){
							msg_counter++;
							resend(args->gsock,msg);
						}
					}
					else if(reliable_list_ret==EMPTY_BODY){
						//Message exists in the message list but with empty payload.Update info
						//Resend the message using reliable multicast group messaging technique
						delivery_num=copy_message_body(msg.sender_pid,msg.sequence_number,msg.payload);
						if(msg.sender_pid!=args->myid){
							msg_counter++;
							resend(args->gsock,msg);
						}
						
						if(blocked_member==1 && delivery_num==deliver_to_api_order){
							blocked_member=0;
							mysem_up(receive_sem);
						}
					}
				}
			}
			sock = socket(AF_INET, SOCK_DGRAM, 0);
			if (sock == -1) { 
				printf("Ack Socket creation failed\n"); 
				exit(1); 
			} 
			msg.type=htonl(ACK_MSG_TYPE);
			msg.sender_pid=htonl(msg.sender_pid);
			msg.sequence_number=htonl(msg.sequence_number);
			
			//Send an ack message to the message sender
			msg_counter++;
			if(sendto(sock,&msg,sizeof(struct send_message),0,(struct sockaddr*)&ack,sizeof(ack)) < 0){
				fprintf(stderr,"Sending ack message error\n");
			}
			close(sock);
		}
		else if(msg.type==SEQ_TYPE){
			if(deliver_to_api_order<=msg.delivery_number){
				//Got a sequence message
				reliable_list_ret=search_node_without_seq_num(msg.sender_pid,msg.sequence_number);
				if(reliable_list_ret==NODE_WITHOUT_SEQNUM_EXISTS){
					//Message exists in the message list,but it has not sequence number.Update info
					copy_seqnum(msg.sender_pid,msg.sequence_number,msg.delivery_number,msg.sequencer_sender_id);
					//Resend the sequence message using reliable multicast group messaging technique
					if(msg.sequencer_sender_id!=args->myid){
						msg_counter++;
						resend(args->gsock,msg);
					}
					if(blocked_member==1 && msg.delivery_number==deliver_to_api_order){
						blocked_member=0;
						mysem_up(receive_sem);
					}
				}
				else if(reliable_list_ret==NODE_WITHOUT_SEQNUM_NOT_EXISTS){
					//Got a sequence message,but message does not exist in the message list.Add it with empty body
					add_node_without_body(msg.sender_pid,msg.sequence_number,msg.delivery_number,msg.sequencer_sender_id);
					//Resend the sequence message using reliable multicast group messaging technique
					if(msg.sequencer_sender_id!=args->myid){
						msg_counter++;
						resend(args->gsock,msg);
					}
				}
			}
			sock = socket(AF_INET, SOCK_DGRAM, 0);
			if (sock == -1) { 
				printf("Ack Socket creation failed\n"); 
				exit(1); 
			} 
			msg.type=htonl(ACK_SEQ_TYPE);
			msg.sender_pid=htonl(msg.sender_pid);
			msg.sequence_number=htonl(msg.sequence_number);
			msg_counter++;
			//Send an ack message to the sequence message sender
			if(sendto(sock,&msg,sizeof(struct send_message),0,(struct sockaddr*)&ack,sizeof(ack)) < 0){
				fprintf(stderr,"Sending ack message error\n");
			}
			close(sock);
		}
		memset((struct send_message *)&msg,0,sizeof(struct send_message));
		memset((struct sockaddr_in *)&ack,0,sizeof(struct sockaddr_in));
		mysem_up(mtx);
	}
	return NULL;
}
void *ACK_Receiver(){
	
	struct send_message msg;
	
	while(1){
		if(recvfrom(ack_sock,&msg,sizeof(struct send_message),0,NULL,NULL)<0){
			printf("Problem with recvfrom\n");
			return NULL;
		}
		mysem_down(mtx);
		msg.type=ntohl(msg.type);
		msg.sender_pid=ntohl(msg.sender_pid);
		msg.sequence_number=ntohl(msg.sequence_number);
		msg.delivery_number=ntohl(msg.delivery_number);
		if(msg.type==ACK_MSG_TYPE){
			//Received a message ack
			update_msg_ack_counter(msg.sender_pid,msg.sequence_number,members);
		}
		else if(msg.type==ACK_SEQ_TYPE){
			//Rreceived a sequence ack
			update_seq_ack_counter(msg.sender_pid,msg.sequence_number,members);
		}
		mysem_up(mtx);
	}
}
void *polling_thread(void *arg){
	
	int polling_sock;
	
	struct send_message polling_reply;
	sigset_t signal_mask;
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask,SIGALRM);
	pthread_sigmask(SIG_BLOCK,&signal_mask,NULL);
	struct join_struct polling_msg;
	polling_msg.member_id=htonl(global_myid);
	polling_msg.type=htonl(POLLING_TYPE);
	
	while(1){
		sleep(POLLING_TIME);
		polling_sock = socket(AF_INET, SOCK_STREAM, 0); 
		if (polling_sock == -1) { 
			printf("Polling TCP Socket creation failed\n"); 
			exit(1); 
		} 
		while(connect(polling_sock,(struct sockaddr *)&servaddr, sizeof(servaddr)) != 0){ 
			printf("Polling connection with the manager failed\n"); 
			//exit(1); 
		}
		send(polling_sock,&polling_msg,sizeof(struct join_struct),0);
		recv(polling_sock,&polling_reply,sizeof(struct send_message),0);
		if(ntohl(polling_reply.sequence_number)!=0){
			members=ntohl(polling_reply.sequence_number);
		}
		if(strcmp(polling_reply.payload,"No updates found")!=0){
            printf(ANSI_COLOR_GREEN"%s,member's id:%d,total members in the group:%d\n"ANSI_COLOR_RESET,polling_reply.payload,ntohl(polling_reply.sender_pid),members);
			if(strcmp("A new member was left the group",polling_reply.payload)==0){
				garbage_collect(deliver_to_api_order,min_k_head);
                min_k_delete();
				sequencer_order=0;
				seqno=0;
				deliver_to_api_order=1;
				
				if(ntohl(polling_reply.sequencer_sender_id)==1){
					is_sequencer=1;
					printf("I am the new sequencer\n");
				}
				
			}
			else{
                garbage_collect(deliver_to_api_order,min_k_head);
				min_k_delete();
				seqno=0;
				sequencer_order=0;
				deliver_to_api_order=1;
			}
        }
		close(polling_sock);
	}
	return(NULL);
}
