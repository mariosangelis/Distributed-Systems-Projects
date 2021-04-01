/*Ack message from server to client.It includes a string(ACK or NACK) and a request id produced by server*/
struct ack_struct{
	int type;
	char reply_message[5];
	int id;
	int client_unique_id;
};
/*Final_struct is the reply struct received by receiver*/
struct final_struct{
	int type;
	int client_unique_id;
	int reqid;
	char payload[1024];
	char ack_addr[100];
	int ack_port;
};
/*Buffer_struct is used from the client process to pass address,port and data-payload to send-request thread*/
struct buffer_struct{
	char multicast_address[64];
	char my_address[64];
	int port;
	int svcid;
	char payload[1024];
};
/*Request message.It includes service id,a resend_id used in case of server failure,a server unique_id used from the load-balancer and the data-payload*/
struct S{
	int type;
	int discover;
	int svcid;
	int resend_id;
	int unique_id;
	int client_unique_id;
	char ack_addr[100];
	int ack_port;
	int reply_ack_port;
	char payload[1024];
};
/*Discovery_ack struct includes a unique_id produced by server and an integer which symbolizes the number of non-confirmed requests fo this server*/
struct discovery_ack{
	int type;
	int unique_id;
	int capacity;
	char ack_addr[100];
	int ack_port;
};
struct Send_child{
	struct S sendreq_child;
	int reqid;
};
