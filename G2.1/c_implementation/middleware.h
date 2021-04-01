/*Angelis Marios-Kasidakis Theodoros*/
/*AEM : 2406-2258*/
#include <pthread.h>
#include <signal.h>
#include "struct_lib.h"
#include "list.h"
#include "semaphores.h"
#include "broker_list.h"
#include "reply_list.h"
#include "resend_list.h"
#define RETRANSMISION_TIME 25
#define DISCOVERY_TYPE 44
#define REPLY_TYPE 55
#define REQUEST_TYPE 66
#define ACK_TYPE 77
#define MAX_RETRANSMIT 10
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_GREEN   "\e[0;32m"

int lock,block_flag,block_sem,blocked_reqid,open_receiver=0,unique_client_id,my_id,request_receiving_sock,reply_receive_sock;
struct sockaddr_in server_request,client_reply;
char myip[64];
pthread_t thread,discover_thread;
void *server_thread_receieve(void *arg);
void *discovery_receive_thread(void *arg);
void *thread_resend(void *arg);
int registerr(void *arg);
int unregister(int svcid);
int getRequest (int svcid, void*buf, int *len);
int sendRequest (int svcid,void *buf,int len);
int getReply (int reqid,void *buf,int len,int block);
void sendReply (int reqid, void *buf, int len);
void *Receiver();
void handler(int signum){
	pthread_exit(NULL);
}
int registerr(void *arg){
	int iret,iret2,len;
	struct sigaction act;
	struct buffer_struct register_buf;
	memcpy(&register_buf,arg,sizeof(struct buffer_struct));
	
	
	act.sa_handler=handler;
	sigaction(SIGUSR1,&act,NULL);
	srand(getpid());
	my_id=rand();
	
	strcpy(myip,register_buf.my_address);
	struct sockaddr_in my_addr;
	len= sizeof(my_addr);
	
	request_receiving_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(request_receiving_sock < 0){
		fprintf(stderr,"Opening request datagram socket error.\n");
		exit(1);
	}
	else{printf("Opening request datagram socket....OK.\n");}
	server_request.sin_family = AF_INET;
	server_request.sin_port = htons(0);
	inet_aton(myip,&server_request.sin_addr);                         //allagi tou address kai tou port,na to pairnv apo orisma
	
	if(bind(request_receiving_sock, (struct sockaddr*)&server_request, sizeof(server_request))){
		fprintf(stderr,"Binding request datagram socket error.\n");
		exit(1);
	}
	else{printf("Binding request datagram socket...OK.\n");}
	
	getsockname(request_receiving_sock, (struct sockaddr *) &my_addr,(socklen_t *)&len);
	//printf("%d\n",ntohs(my_addr.sin_port));
	
	/*Create a thread for receiving requests from clients*/
	iret = pthread_create( &thread, NULL,server_thread_receieve,(void*)(arg));         
	if(iret){
		fprintf(stderr,"Error - pthread_create() return code: %d\n",iret);
		exit(EXIT_FAILURE);
	}
	iret2 = pthread_create( &discover_thread, NULL,discovery_receive_thread,(void*)(arg));         
	if(iret2){
		fprintf(stderr,"Error - pthread_create() return code: %d\n",iret2);
		exit(EXIT_FAILURE);
	}
	return 0;
}
int getRequest (int svcid, void*buf, int *len){
	/*Get the first request from servers buffer(FCFS)*/
	if(head->next!=head){
		struct final_struct copy;
		memcpy(copy.payload,head->prev->payload,1024);
		copy.reqid=head->prev->reqid;
		copy.client_unique_id=head->prev->client_unique_id;
		memcpy(buf,&copy,sizeof(struct final_struct));
		int retid=head->prev->reqid;
		return retid;
	}
	return -1;
}              
int getReply (int reqid,void *buf,int len,int block){
	
	struct reply_node *search_reply;
	/*If reply list is not empty,check if there is a reply for request with id equal to reqid*/
	mysem_down(lock);
	if(reply_head->next!=reply_head){
		search_reply=reply_list_search(reqid);
		if(search_reply==NULL && block==0){
			mysem_up(lock);
			return (-1);
		}
		else if(search_reply==NULL && block==1){
			block_flag=1;
			block_sem=mysem_create(block_sem,0);
			blocked_reqid=reqid;
			printf("BLOCKING\n");
			mysem_up(lock);
			mysem_down(block_sem);
			mysem_down(lock);
			block_flag=0;
			blocked_reqid=0;
			search_reply=reply_list_search(reqid);
			mysem_destroy(block_sem);
		}
		/*Copy the reply message to buffer and delete reply from reply list*/
		memcpy(buf,search_reply->payload,1024);
		reply_list_delete(reqid);
		mysem_up(lock);
		return(0);
	}
	else if(reply_head->next==reply_head && block==1){
		block_flag=1;
		block_sem=mysem_create(block_sem,0);
		blocked_reqid=reqid;
		printf("BLOCKING\n");
		mysem_up(lock);
		mysem_down(block_sem);
		mysem_down(lock);
		block_flag=0;
		blocked_reqid=0;
		search_reply=reply_list_search(reqid);
		mysem_destroy(block_sem);
		/*Copy the reply message to buffer and delete reply from reply list*/
		memcpy(buf,search_reply->payload,1024);
		reply_list_delete(reqid);
		mysem_up(lock);
		return(0);
	}
	mysem_up(lock);
	return -1;
}
void *Receiver(void *arg){
	struct buffer_struct receive_buf;
	struct sockaddr_in from;
	int sock;
	struct final_struct server_struct;   
	struct ack_struct reply_ack;
	
	memcpy(&receive_buf,arg,sizeof(struct buffer_struct));
	lock=mysem_create(lock,1);
	resend_head=resend_init_list();
	reply_head=reply_init_list();
	
	
	while(1){
		/*Receive a reply message from server and add it in reply list*/
		recvfrom(reply_receive_sock,&server_struct,sizeof(struct final_struct),0,NULL,NULL);
		if(ntohl(server_struct.type)==REPLY_TYPE && ntohl(server_struct.client_unique_id)==unique_client_id){
			mysem_down(lock);
			printf(ANSI_COLOR_BLUE"Received a number\n"ANSI_COLOR_RESET);
			server_struct.reqid=ntohl(server_struct.reqid);
			reply_list_add(server_struct.payload,server_struct.reqid);   
			
			/*By changing the resend bit of the just added reply in the reply list,we inform the resend thread of the request that reply just came.*/
			setflag(server_struct.reqid);
			strcpy(reply_ack.reply_message,"ACK");
			reply_ack.id=htonl(-1);
			reply_ack.type=htonl(ACK_TYPE);
			
			from.sin_family = AF_INET;
			inet_aton(server_struct.ack_addr,&from.sin_addr);
			from.sin_port =server_struct.ack_port;
			
			sock = socket(AF_INET, SOCK_DGRAM, 0);
			if(sock < 0){
				fprintf(stderr,"Opening datagram socket error.Termination");
				exit(1);
			}
			/*Send a reply ack message to server*/
			sendto(sock,&reply_ack,sizeof(struct ack_struct),0,(struct sockaddr*)&from,sizeof(from));
			close(sock);
			if(block_flag==1 && server_struct.reqid==blocked_reqid){
				printf("UNBLOCKING\n");
				mysem_up(block_sem);
			}
			mysem_up(lock);
		}
		else{printf("Drop out packet\n");}
	}
	return(NULL);
}
int unregister(int svcid){
	
	list_clear();
	free(head);
	pthread_kill(thread,SIGUSR1);
	printf("Unregister is sending a signal to request-receiver thread\n");
	return 0;
}
int sendRequest (int svcid,void *buf,int len){
	
	int sock,servers_on=0,iret,retransmit=0,i,receiving_sock,req_sock,ack_port,len2,len3;
	char ack_addr[100];
	pthread_t thread;
	struct buffer_struct server_buffer;
	struct S client_send,discover_message;
	struct sockaddr_in client,client_req,server_req,my_addr,my_addr2;
	struct discovery_ack discovery_reply;
	struct ack_struct client_ack;
	struct balance_node *head;
	struct timeval read_timeout;
	
	len2 = sizeof(my_addr);
	len3 = sizeof(my_addr);
	memcpy(&server_buffer,buf,len);
	/*Create a socket to send request*/
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0){
		fprintf(stderr,"Opening datagram socket error.Termination");
		exit(1);
	}
	if(open_receiver==0){
		open_receiver=1;
		srand(time(NULL));
		unique_client_id=rand();
		printf("Client id is %d\n",unique_client_id);
		
		strcpy(myip,server_buffer.my_address);
		//Create a udp socket for receiving ack messages
		reply_receive_sock = socket(AF_INET, SOCK_DGRAM, 0);
		if(reply_receive_sock < 0){
			fprintf(stderr,"Opening reply datagram socket error.\n");
			exit(1);
		}
		//else{printf("Opening reply datagram socket....OK.\n");}
		client_reply.sin_family = AF_INET;
		client_reply.sin_port = htons(0);
		
		client_reply.sin_addr.s_addr=htonl(INADDR_ANY);
		
		if(bind(reply_receive_sock, (struct sockaddr*)&client_reply, sizeof(client_reply))){
			fprintf(stderr,"Binding reply datagram socket error.\n");
			exit(1);
		}
		//else{printf("Binding reply datagram socket...OK.\n");}
		
		iret = pthread_create(&thread, NULL,Receiver,(void *)&server_buffer); 
		if(iret){
			fprintf(stderr,"Error - pthread_create() return code: %d\n",iret);
			exit(EXIT_FAILURE);
		}
		//sleep(10);
	}
	memset((char *) &client, 0, sizeof(client));
	client.sin_family = AF_INET;
	inet_aton(server_buffer.multicast_address,&client.sin_addr);
	client.sin_port = htons(server_buffer.port);
	
	//Create a udp socket for receiving ack messages
	receiving_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(receiving_sock < 0){
		fprintf(stderr,"Opening request datagram socket error.\n");
		exit(1);
	}
	//else{printf("Opening request datagram socket....OK.\n");}
	client_req.sin_family = AF_INET;
	client_req.sin_port = htons(0);
	//client_req.sin_addr.s_addr=htonl(INADDR_ANY);
	inet_aton(myip,&client_req.sin_addr); 
	
	read_timeout.tv_sec = 4;
	read_timeout.tv_usec = 0;
	setsockopt(receiving_sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
	
	if(bind(receiving_sock, (struct sockaddr*)&client_req, sizeof(client_req))){
		fprintf(stderr,"Binding request datagram socket error.\n");
		exit(1);
	}
	//else{printf("Binding request datagram socket...OK.\n");}
	
	
	for(i=0;i<MAX_RETRANSMIT;i++){
		/*Send a discovery message to the multicast group*/
		balance_init_list(&head);
		servers_on=0;
		discover_message.svcid=htonl(svcid);
		discover_message.discover=htonl(1);
		discover_message.type=htonl(DISCOVERY_TYPE);
		
		getsockname(receiving_sock, (struct sockaddr *) &my_addr,(socklen_t *)&len2);
		//printf("%d\n",ntohs(my_addr.sin_port));
		discover_message.ack_port=my_addr.sin_port;
		strcpy(discover_message.ack_addr,"192.168.1.102");
		
		if(sendto(sock,&discover_message,sizeof(struct S),0,(struct sockaddr*)&client,sizeof(client)) < 0){fprintf(stderr,"Sending discovery message error");}
		else{printf(ANSI_COLOR_RED"Sending discovery message...OK\n"ANSI_COLOR_RESET);}
		
		while(1){
			/*Get discovery reply messages from all servers*/
			if(recvfrom(receiving_sock,&discovery_reply,sizeof(struct discovery_ack),0,NULL,NULL)<0){
				printf(ANSI_COLOR_RED"Discovery Timeout\n"ANSI_COLOR_RESET);
				/*If all servers sent their discovery reply messages,break*/
				break;
			}
			else{
				if(ntohl(discovery_reply.type)==DISCOVERY_TYPE){
					/*Add each discovery reply message in the balance list*/
					printf(ANSI_COLOR_RED"Got a discover reply message from server %d with capacity %d\n"ANSI_COLOR_RESET,ntohl(discovery_reply.unique_id),ntohl(discovery_reply.capacity));
					balance_list_add(&head,ntohl(discovery_reply.unique_id),ntohl(discovery_reply.capacity),discovery_reply.ack_addr,discovery_reply.ack_port);
					servers_on++;
				}
			}
		}
		if(servers_on!=0){
			/*Find the server with the smaller capacity and send the request to this server*/
			client_send.unique_id=htonl(balance_list_search(&head,ack_addr,&ack_port));
			client_send.svcid=htonl(svcid);
			client_send.client_unique_id=htonl(unique_client_id);
			client_send.resend_id=htonl(0);
			client_send.type=htonl(REQUEST_TYPE);
			strncpy(client_send.payload,server_buffer.payload,1024);
			strcpy(client_send.ack_addr,"192.168.1.102");
			client_send.ack_port=my_addr.sin_port;
			
			
			getsockname(reply_receive_sock, (struct sockaddr *) &my_addr2,(socklen_t *)&len3);
			//printf("%d\n",ntohs(my_addr2.sin_port));
			client_send.reply_ack_port=my_addr2.sin_port;
			
			req_sock = socket(AF_INET, SOCK_DGRAM, 0);
			if (req_sock == -1) { 
				printf("Ack Socket creation failed\n"); 
				exit(1); 
			} 
			server_req.sin_family = AF_INET;
			inet_aton(ack_addr,&server_req.sin_addr);
			server_req.sin_port=ack_port;
			
			if(sendto(req_sock,&client_send,sizeof(struct S),0,(struct sockaddr*)&server_req,sizeof(server_req)) < 0){fprintf(stderr,"Sending datagram message error\n");}
			else{printf("Sending datagram message to server %d\n",ntohl(client_send.unique_id));}
			close(req_sock);
			while(1){
				if(recvfrom(receiving_sock,&client_ack,sizeof(struct ack_struct),0,NULL,NULL)<0){
					printf("Request Timeout\n");
					balance_list_delete(&head);
					retransmit=1;
					break;
				}
				else {
					if(ntohl(client_ack.type)==ACK_TYPE && ntohl(client_ack.client_unique_id)==unique_client_id){
						//printf("%s with id:%d\n",client_ack.reply_message,ntohl(client_ack.id));
						retransmit=0;
						break;
					}
				}
			}
			if(retransmit==0){
				/*Add the request to resend list*/
				resend_list_add(0,ntohl(client_ack.id));
				balance_list_delete(&head);
				break;
			}
		}
	}
	close(sock);
	close(receiving_sock);
	/*Create a resend thread for resending requests in case of server failure*/
	//child.sendreq_child=client_send;
	//child.reqid=ntohl(client_ack.id);
	
	//iret = pthread_create( &thread, NULL,thread_resend,(void *)&child);
	//if(iret){
	//	fprintf(stderr,"Error - pthread_create() return code: %d\n",iret);
	//	exit(EXIT_FAILURE);
	//}
	return (ntohl(client_ack.id));
}
void sendReply (int reqid, void *buf, int len){
	
	int sock,i,j,break_point=0,ack_port,reply_ack_sock,len2;
	char ack_addr[100];
	struct final_struct send_reply_message;
	struct ack_struct reply_ack;
	struct sockaddr_in client_rep,ack_reply,my_addr;
	struct timeval read_timeout;
	len2 = sizeof(my_addr);
	
	reply_ack_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(reply_ack_sock < 0){
		fprintf(stderr,"Opening reply ack datagram socket error.\n");
		exit(1);
	}
	//else{printf("Opening reply ack datagram socket....OK.\n");}
	ack_reply.sin_family = AF_INET;
	ack_reply.sin_port = htons(0);
	inet_aton(myip,&ack_reply.sin_addr);        
	
	read_timeout.tv_sec = 4;
	read_timeout.tv_usec = 0;
	setsockopt(reply_ack_sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
	
	if(bind(reply_ack_sock, (struct sockaddr*)&ack_reply, sizeof(ack_reply))){
		fprintf(stderr,"Binding reply ack datagram socket error.\n");
		exit(1);
	}
	//else{printf("Binding reply ack datagram socket...OK.\n");}
	//printf("searching reqid=%d\n",reqid);
	list_search(reqid,ack_addr,&ack_port);
	
	memcpy(&send_reply_message,buf,len);
	send_reply_message.type=htonl(REPLY_TYPE);
	send_reply_message.reqid=htonl(send_reply_message.reqid);
	send_reply_message.client_unique_id=htonl(send_reply_message.client_unique_id);
	
	getsockname(reply_ack_sock, (struct sockaddr *) &my_addr,(socklen_t *)&len2);
	//printf("%d\n",ntohs(my_addr.sin_port));
	send_reply_message.ack_port=my_addr.sin_port;
	strcpy(send_reply_message.ack_addr,"192.168.1.102");
	list_pop();
	
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0){
		fprintf(stderr,"Opening datagram socket error.Termination");
		exit(1);
	}       
	client_rep.sin_family = AF_INET;
	inet_aton(ack_addr,&client_rep.sin_addr);
	client_rep.sin_port =ack_port;
	
	for(i=0;i<MAX_RETRANSMIT;i++){
		/*Server is sending reply message until receiving a reply ack.Timer is set to 4 seconds.Max retransmitions are 4*/
		if(sendto(sock,&send_reply_message,len,0,(struct sockaddr*)&client_rep,sizeof(client_rep)) < 0){fprintf(stderr,"Sending reply message error\n");}
		else{printf("Sending reply message...OK\n");}
		for(j=0;j<MAX_RETRANSMIT;j++){
			if(recvfrom(reply_ack_sock,&reply_ack,sizeof(struct ack_struct),0,NULL,NULL)<0){printf("Reply_Ack Timeout\n");}
			else{
				if(ntohl(reply_ack.type)==ACK_TYPE){
					printf("Reply %s\n",reply_ack.reply_message);
					break_point=1;
					close(sock);
					close(reply_ack_sock);
					break;
				}
			}
		}
		if(break_point==1){break;}
	}
}
void *discovery_receive_thread(void *arg){
	
	int server_discovery_sock,fromlen,sock,svcid,len;
	struct discovery_ack discovery_reply;
	struct sockaddr_in server_discovery,from,discover_ack,my_addr;
	struct buffer_struct register_buf;
	struct ip_mreq group;
	
	len = sizeof(my_addr);
	memcpy(&register_buf,arg,sizeof(struct buffer_struct));
	svcid=register_buf.svcid;
	
	server_discovery_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(server_discovery_sock < 0){
		fprintf(stderr,"Opening datagram socket error.\n");
		exit(1);
	}
	else{printf("Opening datagram socket....OK.\n");}
	u_int yes = 1;
	/*Multiple servers can receive requests to the same port*/
	if(setsockopt(server_discovery_sock, SOL_SOCKET, SO_REUSEADDR, (char*) &yes, sizeof(yes)) < 0){
		perror("Reusing ADDR failed");
		return NULL;
	}
    
	server_discovery.sin_family = AF_INET;
	server_discovery.sin_port = htons(register_buf.port);
	server_discovery.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(server_discovery_sock, (struct sockaddr*)&server_discovery, sizeof(server_discovery))){
		fprintf(stderr,"Binding datagram socket error.\n");
		exit(1);
	}
	else{printf("Binding datagram socket...OK.\n");}
	head=init_list();
	fromlen=sizeof(struct sockaddr_in);
	group.imr_multiaddr.s_addr = inet_addr(register_buf.multicast_address);
	group.imr_interface.s_addr = htonl(INADDR_ANY);  
	/*Adding membership from multicast group*/
	if(setsockopt(server_discovery_sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,&group, sizeof(group)) < 0){
		perror("Adding multicast group error");
		exit(1);
	}
	else{printf("Adding multicast group...OK.\n");}
	
	while(1){
		struct S *server_struct=(struct S *)malloc(sizeof(struct S));
		/*Receiving a request or a discovery message from a client*/
		if(recvfrom(server_discovery_sock,server_struct,sizeof(struct S),0,(struct sockaddr *)&from,(socklen_t *)&fromlen)<=0){printf("An error has occured in recvfrom\n");}
		if(ntohl(server_struct->type)==DISCOVERY_TYPE){ 
			server_struct->svcid=ntohl(server_struct->svcid);
			server_struct->client_unique_id=ntohl(server_struct->client_unique_id);
			server_struct->unique_id=ntohl(server_struct->unique_id);
			server_struct->resend_id=ntohl(server_struct->resend_id);
			/*If request's id is -1,request is a discovery message*/
			if(ntohl(server_struct->discover)==1 && server_struct->svcid==svcid){
				/*In case of a discovery message,produce a unique id and send it back to client with a discovery ack message*/
				printf(ANSI_COLOR_RED"Got a discover_message,my_id is %d and my capacity is %d\n"ANSI_COLOR_RESET,my_id,list_capacity());
				
				discover_ack.sin_family = AF_INET;
				inet_aton(server_struct->ack_addr,&discover_ack.sin_addr);
				discover_ack.sin_port = server_struct->ack_port;
				
				discovery_reply.unique_id=htonl(my_id);
				discovery_reply.type=htonl(DISCOVERY_TYPE);
				discovery_reply.capacity=htonl(list_capacity());
				
				getsockname(request_receiving_sock, (struct sockaddr *) &my_addr,(socklen_t *)&len);
				//printf("%d\n",ntohs(my_addr.sin_port));
				discovery_reply.ack_port=my_addr.sin_port;
				strcpy(discovery_reply.ack_addr,"192.168.1.102");
				
				sock = socket(AF_INET, SOCK_DGRAM, 0);
				if (sock == -1) { 
					printf("Ack Socket creation failed\n"); 
					exit(1); 
				} 
				
				sendto(sock,&discovery_reply,sizeof(struct discovery_ack),0,(struct sockaddr*)&discover_ack,sizeof(discover_ack));
				close(sock);
			}
		}
	}
}
void *server_thread_receieve(void *arg){
	
	int svcid,fromlen,sock;
	struct buffer_struct register_buf;
	memcpy(&register_buf,arg,sizeof(struct buffer_struct));
	svcid=register_buf.svcid;
	struct sockaddr_in from,request_ack;
	struct ack_struct server_ack;
	
	while(1){
		struct S *server_struct=(struct S *)malloc(sizeof(struct S));
		/*Receiving a request or a discovery message from a client*/
		if(recvfrom(request_receiving_sock,server_struct,sizeof(struct S),0,(struct sockaddr *)&from,(socklen_t *)&fromlen)<=0){printf("An error has occured in recvfrom\n");}
		if(ntohl(server_struct->type)==REQUEST_TYPE){ 
			server_struct->svcid=ntohl(server_struct->svcid);
			server_struct->client_unique_id=ntohl(server_struct->client_unique_id);
			server_struct->unique_id=ntohl(server_struct->unique_id);
			server_struct->resend_id=ntohl(server_struct->resend_id);
			if(server_struct->unique_id!=my_id){printf("Different unique_id,ignore packet\n");}
			else{
				printf("payload is : %s",server_struct->payload);
				printf("svcid is : %d\n",server_struct->svcid);
				if(server_struct->svcid!=svcid){
					printf("Ignore package,faulty service id\n");
					strcpy(server_ack.reply_message,"NACK");
					server_ack.id=-1;
					server_ack.client_unique_id=htonl(server_struct->client_unique_id);
					server_ack.type=htonl(ACK_TYPE);
					request_ack.sin_family = AF_INET;
					inet_aton(server_struct->ack_addr,&request_ack.sin_addr);
					request_ack.sin_port =server_struct->ack_port;
					
					sock = socket(AF_INET, SOCK_DGRAM, 0);
					if (sock == -1) { 
						printf("Ack Socket creation failed\n"); 
						exit(1); 
					} 
					sendto(sock,&server_ack,sizeof(struct ack_struct),0,(struct sockaddr*)&request_ack,sizeof(request_ack));
				}
				else{
					printf("Reading datagram message...OK.\n");
					/*Produce an id for this request and return it to client with an ack message*/
					if(server_struct->resend_id==0){
						srand(time(NULL));
						server_ack.id=rand();
					}
					else{
						/*This line is used in case of server failure.New request id from server,must be the same with the previous one*/
						server_ack.id=server_struct->resend_id;
					}
					/*Add the request in the request buffer*/
					//printf("reply_ack_port=%d\n",server_struct->reply_ack_port);
					//printf("request_id=%d\n",server_ack.id);
					list_add(server_struct->payload,server_ack.id,server_struct->client_unique_id,server_struct->ack_addr,server_struct->reply_ack_port);           
					server_ack.id=htonl(server_ack.id);
					server_ack.type=htonl(ACK_TYPE);
					server_ack.client_unique_id=htonl(server_struct->client_unique_id);
					strcpy(server_ack.reply_message,"ACK");
					/*Send ack message to client*/
					request_ack.sin_family = AF_INET;
					inet_aton(server_struct->ack_addr,&request_ack.sin_addr);
					request_ack.sin_port =server_struct->ack_port;
					
					sock = socket(AF_INET, SOCK_DGRAM, 0);
					if (sock == -1) { 
						printf("Ack Socket creation failed\n"); 
						exit(1); 
					} 
					sendto(sock,&server_ack,sizeof(struct ack_struct),0,(struct sockaddr*)&request_ack,sizeof(request_ack));
				}
			}
			free(server_struct);
		}
	}
	return NULL;
}
void *thread_resend(void *arg){
	/*struct Send_child resend;
	struct balance_node *head;
	int i,N=MAX_RETRANSMIT,sock,servers_on=0,flag,retransmit=0;
	memcpy(&resend,arg,sizeof(struct Send_child));
	
	//In case of server failure,resender thread tries to N times to resend the request
	for(i=0;i<N;i++){
		sleep(RETRANSMISION_TIME);
		printf("CHILD WAKES UP\n");
		//If request's flag is not 0,receiver has received a reply for this request.Thread is exiting
		flag=resend_list_search(resend.reqid);
		if(flag==1){
			printf("No resend,child is exiting...\n");
			resend_list_delete(resend.reqid);
			break;
		}
		printf("Resending...\n");
		//Else,resender thread must resend the request
		balance_init_list(&head);
		struct S discover_message;
		struct sockaddr_in client;
		struct discovery_ack discovery_reply;
		struct ack_struct client_ack;
		struct timeval read_timeout;
		retransmit=0;
		servers_on=0;
		sock = socket(AF_INET, SOCK_DGRAM, 0);
		if(sock < 0){
			fprintf(stderr,"Opening datagram socket error.Termination");
			exit(1);
		}
		read_timeout.tv_sec = 4;
		read_timeout.tv_usec = 0;
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
		memset((char *) &client, 0, sizeof(client));
		client.sin_family = AF_INET;
		inet_aton("239.255.255.250",&client.sin_addr);
		client.sin_port = htons(6000);
		
		//Send a discovery message to the multicast group
		discover_message.svcid=resend.sendreq_child.svcid;
		discover_message.discover=htonl(1);
		discover_message.type=htonl(DISCOVERY_TYPE);
		if(sendto(sock,&discover_message,sizeof(struct S),0,(struct sockaddr*)&client,sizeof(client)) < 0){fprintf(stderr,"Sending discovery message error\n");}
		else{printf(ANSI_COLOR_RED"Sending discovery message...OK\n"ANSI_COLOR_RESET);}
		
		while(1){
			//Get discovery reply messages from all servers
			if(recvfrom(sock,&discovery_reply,sizeof(struct discovery_ack),0,NULL,NULL)<0){
				printf(ANSI_COLOR_RED"Discovery Timeout\n"ANSI_COLOR_RESET);
				//If all servers sent their discovery reply messages,break
				break;
			}
			else{
				if(ntohl(discovery_reply.type)==DISCOVERY_TYPE){
					//Add each discovery reply message in the balance list
					printf(ANSI_COLOR_RED"Got a discover reply message from server %d with capacity %d\n"ANSI_COLOR_RESET,ntohl(discovery_reply.unique_id),ntohl(discovery_reply.capacity));
					balance_list_add(&head,ntohl(discovery_reply.unique_id),ntohl(discovery_reply.capacity));
					servers_on++;
				}
			}
		}
		if(servers_on!=0){
			//Find the server with the smaller capacity and send the request to this server
			resend.sendreq_child.unique_id=htonl(balance_list_search(&head));
			resend.sendreq_child.resend_id=htonl(resend.reqid);
			resend.sendreq_child.type=htonl(REQUEST_TYPE);
			if(sendto(sock,&resend.sendreq_child,sizeof(struct S),0,(struct sockaddr*)&client,sizeof(client)) < 0){fprintf(stderr,"Sending datagram message error");}
			else{printf("Sending datagram message to server %d\n",ntohl(resend.sendreq_child.unique_id));}
			
			while(1){
				if(recvfrom(sock,&client_ack,sizeof(struct ack_struct),0,NULL,NULL)<0){
					printf("Request Timeout\n");
					balance_list_delete(&head);
					retransmit=1;
					break;
				}
				else if(ntohl(discovery_reply.type)==ACK_TYPE && ntohl(client_ack.client_unique_id)==unique_client_id){
					printf("%s with id:%d\n",client_ack.reply_message,ntohl(client_ack.id));
					retransmit=0;
					balance_list_delete(&head);
					//Add the request to resend list
					break;
				}
			}
			if(retransmit==0){break;}
		}
	}*/
	return NULL;
}
