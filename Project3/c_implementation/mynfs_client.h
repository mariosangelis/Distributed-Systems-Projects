#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "mynfs_list.h"
#include "struct_lib.h"
#include "mycache.h"
#include <time.h>
#define MAX_DATA_SIZE 1024
#define FILE_NOT_FOUND -12
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_GREEN   "\e[0;32m"
int openRPC(char *fname,int flags,int *fid);
int readRPC(int fid,int pos,void *buf,int *nofbytes,int tmod_read_rpc,int *fsize,char *filename,int flags);
int writeRPC(int fid,int pos,void *buf,int nofbytes,char *filename,int fd_apl,int flags);
int mynfs_set_srv_addr(char *ipaddr,int port);
int mynfs_set_cache(int size,int validity);
int mynfs_open(char *fname,int flags);
int mynfs_read(int fd,void *buf,size_t n);
int mynfs_write(int fd,void *buf,size_t n);
int mynfs_lseek(int fd,off_t offset,int whence);
int mynfs_close(int fd);

int sock,freshness,tmod,time_counter=1;
long int start_time;
struct sockaddr_in nfs_client,nfs_server;
//Set nfs_server address and port
int mynfs_set_srv_addr(char *ipaddr,int port){
	
	nfs_list_init();
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0){
		fprintf(stderr,"Opening datagram socket error.\n");
		exit(1);
	}
	else{printf("Opening datagram socket....OK.\n");}
	nfs_server.sin_family = AF_INET;
	nfs_server.sin_port = htons(port);
	inet_aton(ipaddr,&nfs_server.sin_addr);
	
	return(0);
}
//Initialize cache
int mynfs_set_cache(int size,int validity){
	
	freshness=validity;
	cache_init(size);
	start_time=time(NULL);
	
	return(0);
}
int openRPC(char *fname,int flags,int *fid){
	
	struct nfs_message msg;
	struct nfs_reply reply_msg;
	struct timeval read_timeout;
	int found_reply=0;
	
	strcpy(msg.payload,fname);
	msg.flags=htonl(flags);
	msg.type=htonl(OPEN);
	read_timeout.tv_sec = 4;
	read_timeout.tv_usec = 0;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
	
	while(1){
		//Keep sending the same message with different unique_id until you take a response message with the same unique_id from server
		srand(msg.flags+time_counter);
		msg.unique_id=rand();
		time_counter++;
		msg.unique_id=htonl(msg.unique_id);
		//Send a message which includes file logistics to the server
		if(sendto(sock,&msg,sizeof(struct nfs_message),0,(struct sockaddr*)&nfs_server,sizeof(nfs_server)) < 0){
			fprintf(stderr,"Sending message error\n");
		}
		//Receive a reply message from server including the client_file descriptor for this file
		while(1){
			//Drop out all messages with different unique id(while loop here is to make the program more efficient)
			if(recvfrom(sock,&reply_msg,sizeof(struct nfs_reply),0,NULL,NULL)<0){
				printf(ANSI_COLOR_RED"Server is not responding,resending message...\n"ANSI_COLOR_RESET);
				break;
			}
			if(ntohl(msg.unique_id)==ntohl(reply_msg.unique_id)){
				found_reply=1;
				break;
			}
			else{printf("msg.unique_id=%d,reply.unique_id=%d\n",ntohl(msg.unique_id),ntohl(reply_msg.unique_id));}
		}
		if(found_reply==1){break;}
	}
	printf("Received %d from server\n",ntohl(reply_msg.fd_client));
	*fid=ntohl(reply_msg.fd_client);
	tmod=ntohl(reply_msg.tmod);
	//printf("OpenRPC file_size=%d\n",ntohl(reply_msg.size));
	return(ntohl(reply_msg.size));
}
int readRPC(int fid,int pos,void *buf,int *nofbytes,int tmod_read_rpc,int *fsize,char *filename,int flags){
	
	struct nfs_message msg;
	struct nfs_reply reply_msg;
	struct timeval read_timeout;
	int found_reply=0,new_size,new_client_fd;
	
	msg.fd_client=htonl(fid);
	msg.pos=htonl(pos);
	msg.tmod=htonl(tmod_read_rpc);
	msg.nofbytes=htonl(*nofbytes);
	msg.type=htonl(READ);
	read_timeout.tv_sec = 4;
	read_timeout.tv_usec = 0;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
	
	while(1){
		//Keep sending the same message with different unique_id until you take a response message with the same unique_id from server
		srand(msg.pos+time_counter);
		msg.unique_id=rand();
		time_counter++;
		msg.unique_id=htonl(msg.unique_id);
		
		if(sendto(sock,&msg,sizeof(struct nfs_message),0,(struct sockaddr*)&nfs_server,sizeof(nfs_server)) < 0){
			fprintf(stderr,"Sending read message error\n");
		}
		//Receive a reply message from server including the data
		while(1){
			//Drop out all messages with different unique id(while loop here is to make the program more efficient)
			if(recvfrom(sock,&reply_msg,sizeof(struct nfs_reply),0,NULL,NULL)<0){
				printf(ANSI_COLOR_RED"Server is not responding,resending message...\n"ANSI_COLOR_RESET);
				break;
			}
			if(ntohl(msg.unique_id)==ntohl(reply_msg.unique_id)){
				//If the server has dropped out the file,client must execute openRPC function again
				if(ntohl(reply_msg.type)==FILE_NOT_FOUND){
					new_size=openRPC(filename,flags,&new_client_fd);
					//Server will produce a new fd_client file descriptor.Client must update all the other clients which are using the same file for the new fd_client and the new tmod
					update_fd_client(filename,new_client_fd,flags);
					update_tmod(filename,tmod,new_size);
					update_tcheck(filename,time(NULL)-start_time);
					msg.fd_client=htonl(new_client_fd);
					msg.tmod=htonl(tmod);
					break;
				}
				else if(ntohl(reply_msg.nofbytes)==-1){
					//If an error has occured in the server's read function,return -1 to mynfs_read fucntion
					return(-1);
				}
				else{
					found_reply=1;
					break;
				}
			}
			else{printf("msg.unique_id=%d,reply.unique_id=%d\n",ntohl(msg.unique_id),ntohl(reply_msg.unique_id));}
		}
		if(found_reply==1){break;}
	}
	*fsize=ntohl(reply_msg.size);
	//printf("ReadRPC file_size=%d\n",*fsize);
	memcpy(buf,reply_msg.payload,ntohl(reply_msg.nofbytes));
	*nofbytes=ntohl(reply_msg.nofbytes);
	
	return(ntohl(reply_msg.tmod));
}
int writeRPC(int fid,int pos,void *buf,int nofbytes,char *filename,int fd_apl,int flags){
	
	struct nfs_message msg;
	struct nfs_reply reply_msg;
	struct timeval read_timeout;
	int found_reply=0,new_size,new_client_fd;
	
	memcpy(msg.payload,buf,nofbytes);
	msg.fd_client=htonl(fid);
	msg.pos=htonl(pos);
	msg.nofbytes=htonl(nofbytes);
	msg.type=htonl(WRITE);
	read_timeout.tv_sec = 4;
	read_timeout.tv_usec = 0;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
	
	while(1){
		srand(msg.pos+time_counter);
		msg.unique_id=rand();
		time_counter++;
		msg.unique_id=htonl(msg.unique_id);
		//Send a message which includes file logistics and the write data to the server
		if(sendto(sock,&msg,sizeof(struct nfs_message),0,(struct sockaddr*)&nfs_server,sizeof(nfs_server)) < 0){
			fprintf(stderr,"Sending read message error\n");
		}
		//Receive ack reply from server.Ack includes the new tmod of the file
		while(1){
			//Drop out all messages with different unique id(while loop here is to make the program more efficient)
			if(recvfrom(sock,&reply_msg,sizeof(struct nfs_reply),0,NULL,NULL)<0){
				printf("Server is not responding,resending message...\n");
				break;
			}
			if(ntohl(msg.unique_id)==ntohl(reply_msg.unique_id)){
				//If the server has dropped out the file,client must execute openRPC function again
				if(ntohl(reply_msg.type)==FILE_NOT_FOUND){
					printf("Dont found the file in the server\n");
					new_size=openRPC(filename,flags,&new_client_fd);
					//Server will produce a new fd_client file descriptor.Client must update all the other clients which are using the same file for the new fd_client and the new tmod
					update_fd_client(filename,new_client_fd,flags);
					update_tmod(filename,tmod,new_size);
					update_tcheck(filename,time(NULL)-start_time);
					msg.fd_client=htonl(new_client_fd);
					msg.tmod=htonl(tmod);
					break;
				}
				else if(ntohl(reply_msg.nofbytes)==-1){
					//If an error has occured in the server's write function,return -1 to mynfs_write fucntion
					return(-1);
				}
				else{
					found_reply=1;
					break;
				}
			}
			else{printf("msg.unique_id=%d,reply.unique_id=%d\n",ntohl(msg.unique_id),ntohl(reply_msg.unique_id));}
		}
		if(found_reply==1){break;}
	}
	//Because of tmod change,clear the cache and update the tcheck and the tmod value at all nfs_client list nodes referring to that file
	nofbytes=ntohl(reply_msg.nofbytes);
	nfs_update_pos(fd_apl,pos+nofbytes);
	update_tmod(filename,ntohl(reply_msg.tmod),ntohl(reply_msg.size));
	clear_cache(filename);
	update_tcheck(filename,time(NULL)-start_time);
	
	return(0);
}
//Move the position without sending any message to the server
int mynfs_lseek(int fd,off_t offset,int whence){
	
	int send_fid,tmod_nfs_read,file_size,pos,flags;
	long int tcheck_nfs_read;
	char filename[100];
	
	nfs_list_search(fd,&send_fid,&pos,filename,&tcheck_nfs_read,&tmod_nfs_read,&file_size,&flags);
	if(send_fid==-1){
		return(-1);
	}
	if(whence==SEEK_SET){
		nfs_update_pos(fd,offset);
		printf("new position is %ld\n",offset);
	}
	else if(whence==SEEK_CUR){
		nfs_update_pos(fd,pos+offset);
		printf("new position is %ld\n",pos+offset);
	}
	else if(whence==SEEK_END){
		nfs_update_pos(fd,file_size+offset);
		printf("new position is %ld\n",file_size-offset);
	}
	return(0);
}
//If a file with the same name and the same flags exists in the nfs_client list,produce a new file descriptor and return it to the client,else make an openRPC call
int mynfs_open(char *fname,int flags){
	
	int fd_client,fd_apl,size,flagopenRPC=0,ret_val;
	//If fd_client==0,there is no one with a file descriptor to the same file and with the same flags,so make an openRPC call
	fd_client=search_file_name(fname,&tmod,&size,flags);
	if(fd_client==0 || flags==(O_CREAT|O_TRUNC) || flags==(O_RDWR|O_TRUNC) || flags==(O_RDONLY|O_TRUNC) || flags==(O_WRONLY|O_TRUNC)){
		size=openRPC(fname,flags,&fd_client);
		if(fd_client==-1){return(-1);}
		flagopenRPC=1;
	}
	srand(fd_client+time_counter);
	time_counter++;
	fd_apl=rand();

	printf("Return to application fd:%d\n",fd_apl);
	ret_val=fd_apl_to_this_file(fname);
	//open the same file with different flags,update fd_client,tmod,tcheck,filesize,and position to all other clients using the same file descriptor
	if(ret_val>0 && flagopenRPC==1) {
		//Do not update fd_client,beacause of different flags
		printf("size=%d\n",size);
		update_tmod(fname,tmod,size);
		clear_cache(fname);
		update_tcheck(fname,time(NULL)-start_time);
	}
	nfs_list_add(fname,fd_apl,fd_client,tmod,time(NULL)-start_time,size,flags);
	return(fd_apl);
}
//Read n bytes from file fd,put them in the *buf
int mynfs_read(int fd,void *buf,size_t n){
	
	int send_fid,pos,fetch_bytes=n,cache_find=0,final_n=n,bytes_before_eof=0,tmod_nfs_read,refreshed_tmod,refreshed_size,filesize,flags;
	long int tcheck_nfs_read;
	char filename[100],*temp_buffer,fetch_buffer[MAX_DATA_SIZE],*temp_start;
	temp_buffer=(char *)malloc(sizeof(char)*(n));
	temp_start=temp_buffer;
	struct cache_block *current;
	
	//If the length of the data is bigger than a cache page,read only a cache page from the server
	if(n>MAX_DATA_SIZE) {
		n=MAX_DATA_SIZE;
		final_n=MAX_DATA_SIZE;
	}
	nfs_list_search(fd,&send_fid,&pos,filename,&tcheck_nfs_read,&tmod_nfs_read,&filesize,&flags);
	if(send_fid==-1){
		return(-1);
	}
	//Check the cache data validity
	if((time(NULL)-start_time)-tcheck_nfs_read>freshness){
		printf(ANSI_COLOR_GREEN"Freshness has expired,check tmod\n"ANSI_COLOR_RESET);
		//Check if the tmod has changed
		refreshed_tmod=readRPC(send_fid,pos,fetch_buffer,&fetch_bytes,tmod_nfs_read,&refreshed_size,filename,flags);
		if(refreshed_tmod<0){
			//If an error has occured in the server's read function,return -1 to application
			return(-1);
		}
		if(refreshed_tmod!=tmod_nfs_read){
			//If tmod was changed,clear the cache and update the tmod value at all nfs_client list nodes referring to that file
			printf("Tmod has changed\n");
			update_tmod(filename,refreshed_tmod,refreshed_size);
			clear_cache(filename);
		}
		//Update the tcheck value at all nfs_client list nodes referring to that file
		update_tcheck(filename,time(NULL)-start_time);
	}
	
	while(1){
		for(current=cache_head->next;;current=current->next){
			if(current==cache_head){break;}
			if(strcmp(current->file_name,filename)==0){
				//All data are located inside this cache page
				if(pos>=current->start_offset && pos<current->end_offset && (pos+n-1)<=current->end_offset){
					memcpy(temp_buffer,&current->data[pos-current->start_offset],n);
					pos+=n;
					printf(ANSI_COLOR_BLUE"cache hit\n"ANSI_COLOR_RESET);
					printf("pos=%d,n=%ld\n",pos,n);
					nfs_update_pos(fd,pos);
					update_LRU_cache(current);
					cache_find++;
					memcpy(buf,temp_start,final_n);
					free(temp_start);
					return(final_n);
				}
				else if(pos>=current->start_offset && pos<=current->end_offset && (pos+n-1)>current->end_offset){
					//Only a part of the data are located inside this cache page
					memcpy(temp_buffer,&current->data[pos-current->start_offset],current->end_offset-pos+1);
					cache_find++;
					bytes_before_eof+=current->end_offset-pos+1;
					temp_buffer+=current->end_offset-pos+1;
					n-=current->end_offset-pos+1;
					pos+=current->end_offset-pos+1;
					nfs_update_pos(fd,pos);
					printf("pos=%d,n=%ld\n",pos,n);
				}
			}
		}
		if(cache_find==0){
			//Dont found data in anyone cache page,so fetch them from server
			nfs_list_search(fd,&send_fid,&pos,filename,&tcheck_nfs_read,&tmod_nfs_read,&filesize,&flags);
			if(send_fid==-1){
				return(-1);
			}
			printf(ANSI_COLOR_BLUE"Fetching bytes from nfs_server\n"ANSI_COLOR_RESET);
			refreshed_tmod=readRPC(send_fid,pos,fetch_buffer,&fetch_bytes,tmod_nfs_read,&refreshed_size,filename,flags);
			if(refreshed_tmod<0){
				//If an error has occured in the server's read function,return -1 to application
				return(-1);
			}
			if(fetch_bytes==0){
				memcpy(buf,temp_start,bytes_before_eof);
				free(temp_start);
				return(bytes_before_eof);
			}
			update_cache(filename,(pos/MAX_DATA_SIZE)*MAX_DATA_SIZE,(pos/MAX_DATA_SIZE)*MAX_DATA_SIZE+fetch_bytes-1,fetch_buffer,fetch_bytes);
		}
		else{cache_find=0;}
	}
	free(temp_start);
	return(final_n);
}
//Write n bytes located to *buf in the file fd
int mynfs_write(int fd,void *buf,size_t n) {
	
	int send_fid,pos,nofbytes=n,tmod_nfs_read,filesize,flags;
	long int tcheck_nfs_read;
	int i,j=0,ret;
	char filename[100];
	//int bytes=0;
	char temp_buffer_for_big_data[MAX_DATA_SIZE];
	char *whole_buf=(char *)buf;
	
	nfs_list_search(fd,&send_fid,&pos,filename,&tcheck_nfs_read,&tmod_nfs_read,&filesize,&flags);
	if(send_fid==-1){
		return(-1);
	}
	//If the length of the data is bigger than a cache page,write the data page to page to the server
	if(n>MAX_DATA_SIZE){
		while(n>MAX_DATA_SIZE) {
			for(i=0;i<MAX_DATA_SIZE;i++,j++) {
				temp_buffer_for_big_data[i]=whole_buf[j];
			}
			printf("pos=%d,n=%ld\n",pos,n);
			ret=writeRPC(send_fid,pos,temp_buffer_for_big_data,MAX_DATA_SIZE,filename,fd,flags);
			if(ret<0){
				//If an error has occured in the server's write function,return -1 to application
				return(-1);
			}
			nfs_list_search(fd,&send_fid,&pos,filename,&tcheck_nfs_read,&tmod_nfs_read,&filesize,&flags);
			n-=MAX_DATA_SIZE;
		}
		ret=writeRPC(send_fid,pos,&whole_buf[nofbytes-n],n,filename,fd,flags);
		if(ret<0){
			//If an error has occured in the server's write function,return -1 to application
			return(-1);
		}
		printf("pos=%d,n=%ld\n",pos,n);
	}
	else {
        printf("pos=%d,n=%ld\n",pos,n);
		ret=writeRPC(send_fid,pos,buf,n,filename,fd,flags);
		if(ret<0){
			//If an error has occured in the server's write function,return -1 to application
			return(-1);
		}
	}
	return(nofbytes);
}
int mynfs_close(int fd){
	int ret;

	ret=delete_nfs_node(fd);
	return(ret);
}
