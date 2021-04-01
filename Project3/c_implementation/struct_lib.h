#define OPEN -10
#define READ -11
#define WRITE -12
#define LSEEK -13
#define MAX_DATA_SIZE 1024
struct nfs_message{
    int type;
	int unique_id;
	char payload[MAX_DATA_SIZE];
	int flags;
    int tmod;
    int fd_client;
    int nofbytes;
    int pos;
};
struct nfs_reply{
    int type;
	int unique_id;
	int size;
	int fd_client;
	int pos;
    int nofbytes;
    int tmod;
    char payload[MAX_DATA_SIZE];
};
