int diff_binary_files(char *fname1,char *fname2) {

	int fd1;
	int fd2;
	char data1[1];
	char data2[1];
	int read_ret1;
	int read_ret2;

	fd1=open(fname1,O_RDONLY,S_IRWXU);
	fd2=open(fname2,O_RDONLY,S_IRWXU);

	if(fd1==-1) {
		fprintf(stderr,"Error with open()\n");
		exit(EXIT_FAILURE);
	}
	if(fd2==-1) {
		fprintf(stderr,"Error with open()\n");
		exit(EXIT_FAILURE);
	}

	while(1) {
		read_ret1=read(fd1,data1,1);
		read_ret2=read(fd2,data2,1);
		if(read_ret1==0 && read_ret2==0){
			printf("EOF\n");
			break;
		}
		if(data1[0]!=data2[0]) {
			return(1);
		}
	}

	return(0);
}
