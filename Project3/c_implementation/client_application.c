#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "mynfs_client.h"
	
int main(int args,char *argv[]){
	
	if(args<3){
		fprintf(stderr,"Wrong number of arguments\n");
		exit(EXIT_FAILURE);
	}
	int set_addr_ret=-1,fd1,nofbytes,fd2,ret,fd3,fd4,fd5,fd6,end1=0,end2=0;
	char data[10000];
	while(set_addr_ret!=0){
		set_addr_ret=mynfs_set_srv_addr(argv[1],atoi(argv[2]));
	}
	mynfs_set_cache(8192,5);
		
	fd1=mynfs_open(argv[3],O_RDWR|O_CREAT);
	if(fd1==-1){
		fprintf(stderr,"Error in my_nfs_open\n");
		exit(EXIT_FAILURE);
	}
	fd2=mynfs_open(argv[3],O_RDWR|O_CREAT);
	if(fd2==-1){
		fprintf(stderr,"Error in my_nfs_open\n");
		exit(EXIT_FAILURE);
	}
	fd3=open("lake1.jpg",O_RDWR|O_CREAT|O_TRUNC,S_IRWXU);
	if(fd3==-1){
		fprintf(stderr,"Error in my_nfs_open\n");
		exit(EXIT_FAILURE);
	}
	fd4=open("lake2.jpg",O_RDWR|O_CREAT|O_TRUNC,S_IRWXU);
	if(fd4==-1){
		fprintf(stderr,"Error in my_nfs_open\n");
		exit(EXIT_FAILURE);
	}
	sleep(30);
	//Read data from file "server_files/lake.jpg" and write the data in the files "lake1.jpg" and "lake2.jpg"
	while(1){
		
		if(end1==0){
			bzero(data,MAX_DATA_SIZE);
			nofbytes=mynfs_read(fd1,data,1024);
			if(nofbytes==1024){
				ret=write(fd3,data,nofbytes);
				if(ret<0){
					printf("Write failed\n");
					exit(EXIT_FAILURE);
				}
			}
			else{
				ret=write(fd3,data,nofbytes);
				if(ret<0){
					printf("Write failed\n");
					exit(EXIT_FAILURE);
				}
				printf("EOF,closing...\n");
				end1=1;
			}
		}
		if(end2==0){
			bzero(data,MAX_DATA_SIZE);
			nofbytes=mynfs_read(fd2,data,1024);
			if(nofbytes==1024){
				ret=write(fd4,data,nofbytes);
				if(ret<0){
					printf("Write failed\n");
					exit(EXIT_FAILURE);
				}
			}
			else{
				ret=write(fd4,data,nofbytes);
				if(ret<0){
					printf("Write failed\n");
					exit(EXIT_FAILURE);
				}
				printf("EOF,closing...\n");
				end2=1;
			}
		}
		if(end1==1 && end2==1){break;}
	}
	//Truncate the file "server_files/s.jpg".Try to read from local file s.jpg and to write to the server file "server_files/s.jpg"
	//If the flags for the server_files/s.png are O_CREAT|O_TRUNC,the first write function will fail
	fd5=mynfs_open("server_files/s.png",O_RDWR|O_CREAT);
	if(fd5==-1){
		fprintf(stderr,"Error in my_nfs_open\n");
		exit(EXIT_FAILURE);
	}
	fd6=open("client_s.png",O_CREAT|O_RDWR);
	if(fd6==-1){
		fprintf(stderr,"Error in my_nfs_open\n");
		exit(EXIT_FAILURE);
	}
	int prev_nof_bytes=0,write_ret_val=0;
	nofbytes=0;
	while(1){
		bzero(data,MAX_DATA_SIZE);
		while(nofbytes<10000){
			prev_nof_bytes=nofbytes;
			nofbytes+=read(fd6,data+nofbytes,10000-nofbytes);
			if(nofbytes<0){
				fprintf(stderr,"Client read error\n");
				exit(EXIT_FAILURE);
			}
			if(prev_nof_bytes==nofbytes){break;}
		}
		write_ret_val=mynfs_write(fd5,data,nofbytes);
		if(write_ret_val<0){
			fprintf(stderr,"Server write error\n");
			exit(EXIT_FAILURE);
		}
		if(prev_nof_bytes==nofbytes){break;}
		nofbytes=0;
	}
	printf("EOF\n");
	return(0);
}
