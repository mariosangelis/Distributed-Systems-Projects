/*Angelis Marios */
/*Kasidakis Theodoros */
/* This is an application which tests our NFS.
Through this application we create a local and a remote copy
of a remote file.Finally we compare those two files */

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
#include "diff_binaries.h"

int main(int argc,char *argv[]) {
	
	if(argc<3){
		fprintf(stderr,"Wrong number of arguments.First argument should be IP address of NFS Server.Second argument should be port number of NFS Server\n");
		exit(EXIT_FAILURE);
	}
	
	int set_addr_ret=-1;
	int fd_wr_local; /* fd for the file which is been written localy  */
	int fd_read_remote; /* fd for the file which we read from the NFS Server and we write it locally */
	int fd_wr_remote; /* fd for the file which we read from the NFS Server and after that we write it to the NFS Server */
	int compare_fd; /*fd for reading the file from NFS Server . We read the same file which was written from us with mynfs_write() */
	int fd_remote_read_after_a_remote_write_and_final_local_write; /* fd for writting the file localy . */
	int bytes_returned;
	int check_system_calls;
	char data[10]; // buffer for transfering bytes
	int diff_ret;
	
	while(set_addr_ret!=0){
		set_addr_ret=mynfs_set_srv_addr(argv[1],atoi(argv[2]));
	}
	
	mynfs_set_cache(8192,5);
	
	fd_wr_local=open("photo_local.jpg",O_RDWR|O_CREAT,S_IRWXU);
	if(fd_wr_local==-1) {
		fprintf(stderr,"Error with open()\n");
		exit(EXIT_FAILURE);
	}
	
	fd_read_remote=mynfs_open("server_files/photo.jpg",O_RDWR);
	if(fd_read_remote==-1) {
		fprintf(stderr,"Error with mynfs_open()\n");
		exit(EXIT_FAILURE);
	}
	
	fd_wr_remote=mynfs_open("server_files/local_to_remote_photo.jpg",O_RDWR|O_CREAT);
	if(fd_wr_remote==-1) {
		fprintf(stderr,"Error with mynfs_open()\n");
		exit(EXIT_FAILURE);
	}
	
	while(1) { /* Start reading from the NFS Server and "break" when you reach an EOF */
		printf("Bytes returned from mynfs_read() ::: %d\n",bytes_returned);
		bytes_returned=mynfs_read(fd_read_remote,data,10);
		if(bytes_returned==10) {
			check_system_calls=write(fd_wr_local,data,bytes_returned); /*write to a local file */
			if(check_system_calls==-1) {
				fprintf(stderr,"Error with write() system call\n");
				exit(EXIT_FAILURE);
			}
			check_system_calls=mynfs_write(fd_wr_remote,data,bytes_returned); /* write to NFS Server */
			if(check_system_calls==-1) {
				fprintf(stderr,"Error with mynfs_write()\n");
				exit(EXIT_FAILURE);
			}
		}
		else if(bytes_returned<10 && bytes_returned>0) {
			check_system_calls=write(fd_wr_local,data,bytes_returned); /*write to a local file */
			if(check_system_calls==-1) {
				fprintf(stderr,"Error with write() system call\n");
				exit(EXIT_FAILURE);
			}
			check_system_calls=mynfs_write(fd_wr_remote,data,bytes_returned); /* write to NFS Server */
			if(check_system_calls==-1) {
				fprintf(stderr,"Error with mynfs_write()\n");
				exit(EXIT_FAILURE);
			}
		}
		else if(bytes_returned==0) {
			break;
		}
	}
	
	compare_fd=mynfs_open("server_files/local_to_remote_photo.jpg",O_RDONLY);
	if(compare_fd==-1) {
		fprintf(stderr,"Error with mynfs_open()\n");
		exit(EXIT_FAILURE);
	}
	
	fd_remote_read_after_a_remote_write_and_final_local_write=open("local_to_remote_photo_final_local.jpg",O_RDWR|O_CREAT,S_IRWXU);
	if(fd_remote_read_after_a_remote_write_and_final_local_write==-1){
		fprintf(stderr,"Error with open()\n");
		exit(EXIT_FAILURE);
	}
	
	while(1) { /* Start reading from the NFS Server and "break" when you reach EOF */
		printf("Bytes returned from mynfs_read() ::: %d\n",bytes_returned);
		bytes_returned=mynfs_read(compare_fd,data,10);
		if(bytes_returned==10) {
			check_system_calls=write(fd_remote_read_after_a_remote_write_and_final_local_write,data,bytes_returned); /*write to a local file */
			if(check_system_calls==-1) {
				fprintf(stderr,"Error with write() system call\n");
				exit(EXIT_FAILURE);
			}
		}
		else if(bytes_returned<10 && bytes_returned>0) {
			check_system_calls=write(fd_remote_read_after_a_remote_write_and_final_local_write,data,bytes_returned); /*write to a local file */
			if(check_system_calls==-1) {
				fprintf(stderr,"Error with write() system call\n");
				exit(EXIT_FAILURE);
			}
		}
		else if(bytes_returned==0) {
			break;
		}
	}
	
	printf(" ## Creating files localy and remotely ...... OK  ##\n");
	printf("## Compare those two files ... ## \n");
	sleep(2);
	
	diff_ret=diff_binary_files("photo_local.jpg","local_to_remote_photo_final_local.jpg");
	if(diff_ret==0) {
		printf("## No difference between the two files ## \n");
	}
	else {
		printf("## Binary files are not the same ##\n");
	}
	
	return(0);
}
