#Asynchronous request-reply model with semantics at most once and load balancing service
#Angelis Marios-2406
import socket
from socket import AF_INET, SOCK_DGRAM
import pickle
import threading
import time
import random
import struct
lock = threading.Lock()
sender_list=[]
receiver_list=[]
reply_list=[]
sequence_number_list=[]
servers_list=[]
MAX_RETRANSMMISIONS=5
MULTICAST_PORT=10000
first,my_service_id,my_ip,my_port,my_reply_ip,my_reply_port,sequence_number,my_private_id,reply_sequence_num,my_server_id=0,0,0,0,0,0,0,0,0,0
MULTICAST_ADDRESS="224.0.0.1"
class message_class:
    
    def __init__(self,seq,number,reply_ip,reply_port,client_id):
        self.sequence = seq
        self.number=number
        self.reply_ip=reply_ip
        self.reply_port=reply_port
        self.client_id=client_id
        self.unique_id=0
        
    def get_sequence(self):
        return(self.sequence)
    
    def get_client_id(self):
        return(self.client_id)
    
    def get_reply_ip(self):
        return(self.reply_ip)
    
    def get_reply_port(self):
        return(self.reply_port)
    
    def get_number(self):
        return(self.number)
    
    def get_unique_id(self):
        return(self.unique_id)
    
    def set_sequence(self,seq):
        self.sequence=seq
    
    def set_unique_id(self,unique_id):
        self.unique_id=unique_id
        
    def set_number(self,number):
        self.number=number

class discovery_reply_class:
    def __init__(self, svcid,capacity,ip,port):
        self.service_id = svcid
        self.capacity=capacity
        self.ip=ip
        self.port=port
        
    def get_service_id(self):
        return(self.service_id)
    
    def get_ip(self):
        return(self.ip)
    
    def get_port(self):
        return(self.port)
    
    def get_capacity(self):
        return(self.capacity)   
        
class discovery_message_class:
    def __init__(self, svcid):
        self.service_id = svcid
        
    def get_service_id(self):
        return(self.service_id)
    
    
    
class ack_message_class:
    def __init__(self,sequence_num):
        self.sequence=sequence_num;
        
    def get_sequence(self):
        return(self.sequence)
    
class reply_message_class:
    def __init__(self, message,port,ip,seq,server_id):
        self.message = message
        self.port=port
        self.ip=ip
        self.sequence=seq
        self.server_id=server_id
        
    def get_message(self):
        return(self.message)
    
    def get_server_id(self):
        return(self.server_id)
    
    def get_port(self):
        return(self.port)
    
    def get_sequence(self):
        return(self.sequence)
        
    def get_ip(self):
        return(self.ip)
    
    
#sendRequest function used from client to send requests using load balancing to balance the general congestion
def sendRequest(message,svcid,reply_port):
    global first
    global my_service_id
    global MULTICAST_PORT
    global MULTICAST_ADDRESS
    global my_reply_ip
    global my_reply_port
    global sequence_number
    global my_private_id
    flag=0
    min_capacity=100000
    
    #Initialize service_id,reply_ip,reply_port and start the reply_receiver thread
    if(first==0):
        first=1
        hostname = socket.gethostname()  
        IPAddr = socket.gethostbyname(hostname)    
        print("Your Computer Name is:" + hostname)    
        print("Your Computer IP Address is:" + IPAddr)
        
        my_service_id=svcid
        my_reply_ip=IPAddr
        my_reply_port=reply_port
        my_private_id=random.random()
        #print("my client id is ",my_private_id)
        thread = threading.Thread(target=reply_receiver,args=())
        thread.start()
    
    discovery_message = discovery_message_class(my_service_id)
    multicast_group = (MULTICAST_ADDRESS, MULTICAST_PORT)

    #Create a  multicast socket
    multicast_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    multicast_sock.settimeout(0.5)
    ttl = struct.pack('b', 1)
    multicast_sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl)
    #print("Send a multicast message,my service id is ",my_service_id)
    #Send a discovery message to the multicast channel.
    discovery_message=pickle.dumps(discovery_message)
    multicast_sock.sendto(discovery_message, multicast_group)
    
    #Wait to receive a reply for the discovery message from each server which serves this specific service id
    while(1):
        discovery_reply=discovery_reply_class(0,0,0,0)
        try:
            data, addr = multicast_sock.recvfrom(1024)
        except socket.timeout:
            #All servers sent a discovery reply message 
            break
        flag=1
        discovery_reply=pickle.loads(data)
        #print("Got a discovery reply.Server's capacity is ",discovery_reply.get_capacity())
        #Find the server with the minimum capacity
        if(discovery_reply.get_capacity()<min_capacity):
            min_capacity=discovery_reply.get_capacity()
            server_address=(discovery_reply.get_ip(),discovery_reply.get_port())
    if(flag==0):
        #None server is available,clear servers list
        servers_list.clear()
        return -1
    #print("Send to server with address ",server_address,"because his capacity is ",min_capacity)
    #Create a UDP socket
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    sock.settimeout(5.0)
    #Sequence_number is used for duplicate packets check.
    sequence_number +=1
    send_message=message_class(sequence_number,message,my_reply_ip,reply_port,my_private_id)
    send_message=pickle.dumps(send_message)
    ackmsg=ack_message_class(0);
    
    #Send the request to the server with the minimum capacity.Use the stop&wait protocol for retransmition
    #If the server is down,we use "at most once" semantics,so the middleware tries for MAX_RETRANSMMISIONS times to resend the request message
    for i in range(0,MAX_RETRANSMMISIONS):
        try:
            sock.sendto(send_message,server_address)
            data, addr = sock.recvfrom(1024)
        except socket.timeout:
            print("Got a timeout,resending")
            continue
        ackmsg=pickle.loads(data)
        #print("Got an ack message for message with sequence",ackmsg.get_sequence())
        if (ackmsg.get_sequence()==sequence_number):
            break
    
#multicast_receiver thread used from server to receive multicast messages
def multicast_receiver():
    global lock
    global MULTICAST_PORT
    global MULTICAST_ADDRESS
    #Create a multicast socket and bind to the multicast address in order to "listen" discovery messages
    multicast_address=(MULTICAST_ADDRESS,MULTICAST_PORT)
    sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(multicast_address)
    
    group = socket.inet_aton(MULTICAST_ADDRESS)
    mreq = struct.pack('4sL', group, socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    
    while 1:
        rcvmsg=discovery_message_class(0)
        data, addr = sock.recvfrom(1024)
        rcvmsg=pickle.loads(data)
        #Send a point to point discovery reply message only if the server can serve this request
        if(rcvmsg.get_service_id()==my_service_id):
            lock.acquire()
            #print("Got a discovery message")
            #Send back to client a discovery reply message which contains the server's capacity,his ip and his port
            discovery_reply_message=discovery_reply_class(my_service_id,len(receiver_list),my_ip,my_port)
            lock.release()
            discovery_reply_message=pickle.dumps(discovery_reply_message)
            sock.sendto(discovery_reply_message,addr)
    
    
#Receiver thread used from server to receive requests
def receiver():
    global lock
    i=0
    #Creates a udp socket and bind to "listen" requests from this channel
    address=(my_ip,my_port)
    sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(address)
    while 1:
        flag=0
        rcvmsg=message_class(0,0,0,0,0)
        data, addr = sock.recvfrom(1024)
        rcvmsg=pickle.loads(data)
        #print("Got a message")
        #Duplicate packets check
        #We use a list(each index contains a list) of lists(each index contains the sequence_number of the most recently read message for each client).
        #ex : This server serves 2 clients.First client has id:2.19234 and second client has id:5.12352.The most recently read message from the first client is a message with sequence_number=2 and the most recently read message from the second client is a message with sequence_number=6.The sequence_number_list will be as follows :
        #[[2.19234,2],[5.12352,6]].This means that the server is waiting for a message from client 1 with sequence_number larger than 2 or a message from client 2 with sequence_number larger than 6
        
        #Check if we have received previous messages from this client.
        for i in range(0,len(sequence_number_list)):
            if(rcvmsg.get_client_id()==sequence_number_list[i][0]):
                flag=1
                break
        #If flag=0,this is the first message from this client,so the server adds the [client_id,sequence_number of this message] list to the sequence_number_list
        if(flag==0):
            #print("New client,add his client_id to the sequence_number_list")
            #If the message has sequence_number=5,add the list [client_id of this client,4] to the sequence_number_list
            sequence_number_list.append([rcvmsg.get_client_id(),rcvmsg.get_sequence()-1])
            served_sequence=rcvmsg.get_sequence()-1
        else:served_sequence=sequence_number_list[i][1]
            
        #The previous ACK message has not arrived to the client,so this request is duplicate.Thus,resend the ACK message
        #ex:sequence_number of this message is 4 and served_sequence is >=4,so resend ACK message
        if(rcvmsg.get_sequence()<=served_sequence):
            message=ack_message_class(rcvmsg.get_sequence())
            message=pickle.dumps(message)
            sock.sendto(message,addr)
        else:
            #ex:sequence_number of this message is 4 and served_sequence is 3,so this is a new message.
            lock.acquire()
            for i in range(0,len(sequence_number_list)):
                if(rcvmsg.get_client_id()==sequence_number_list[i][0]):
                    sequence_number_list[i][1] = rcvmsg.get_sequence()
                    break
                
            
            #print(sequence_number_list)
            #Add the request to the receiver list.Give a unique id in each request
            rcvmsg.set_unique_id(random.random())
            receiver_list.append(rcvmsg)
            print("Got a request for number ",rcvmsg.get_number(),"with seq_id ",rcvmsg.get_sequence(),"reply to ip ",rcvmsg.get_reply_ip(),"and to port ",rcvmsg.get_reply_port())
            lock.release()
            #Send an ACK message back to sender
            message=ack_message_class(rcvmsg.get_sequence())
            message=pickle.dumps(message)
            sock.sendto(message,addr)
    
#reply_receiver thread used from client to receive replies        
def reply_receiver():
    global lock
    global my_reply_ip
    global my_reply_port
    flag=0
    #Create a UDP socket
    address=(my_reply_ip,my_reply_port)
    sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(address)
    while 1:
        flag=0
        served_sequence=0
        rcvmsg=reply_message_class(0,0,0,0,0)
        data, addr = sock.recvfrom(1024)
        rcvmsg=pickle.loads(data)   
        
        #Duplicate packets check
        #We use a list(each index contains a list) of lists(each index contains the ip,the port and the sequence_number of the most recently read reply message for each server).
        #ex : This client "speaks" 2 servers.First server has ip:192.168.1.47 and port:8000 and second server has ip:192.168.1.33 and port:9000.The most recently read reply message from the first server is a message with sequence_number=2 and the most recently read reply message from the second server is a message with sequence_number=6.The servers_list will be as follows :
        #[[192.168.1.47,8000,2],[192.168.1.33,9000,6]].This means that the client is waiting for a reply message from server 1 with sequence_number larger than 2 or a reply message from server 2 with sequence_number larger than 6
        
        #Check if we have received previous reply messages from this server.
        for i in range(0,len(servers_list)):
            if(rcvmsg.get_ip()==servers_list[i][0] and rcvmsg.get_port()==servers_list[i][1] and rcvmsg.get_server_id()==servers_list[i][3]):
                flag=1
                break
        #If flag=0,this is the first message from this server,so the client adds the [server's ip,server's port,sequence_number of this reply message] list to the servers_list
        if(flag==0):
            print("New server,add server ip and server port to the servers_list")
            #If the reply message has sequence_number=5,add the list [server's ip,server's port,4] to the servers_list
            servers_list.append([rcvmsg.get_ip(),rcvmsg.get_port(),rcvmsg.get_sequence()-1,rcvmsg.get_server_id()])
            served_sequence=rcvmsg.get_sequence()-1
        else:served_sequence=servers_list[i][2]
        
        #The previous REPLY ACK message has not arrived to the server,so this reply is duplicate.Thus,resend the REPLY ACK message
        #ex:sequence_number of this reply message is 4 and served_sequence is >=4,so resend REPLY ACK message
        if(rcvmsg.get_sequence()<=served_sequence):
            
            message=ack_message_class(rcvmsg.get_sequence())
            message=pickle.dumps(message)
            sock.sendto(message,addr)
            
        else:
            #ex:sequence_number of this reply message is 4 and served_sequence is 3,so this is a new message.
            lock.acquire()
            for i in range(0,len(servers_list)):
                if(rcvmsg.get_ip()==servers_list[i][0] and rcvmsg.get_port()==servers_list[i][1] and rcvmsg.get_server_id()==servers_list[i][3]):
                    servers_list[i][2] = rcvmsg.get_sequence()
                    break
            reply_list.append(rcvmsg.get_message())
            lock.release()
            #Send a reply ACK message back to server 
            message=ack_message_class(rcvmsg.get_sequence())
            message=pickle.dumps(message)
            sock.sendto(message,addr)
            
#When the client application wants to get a reply for a request which was sent before,it calls the getReply function
def getReply():
    global lock
    lock.acquire()
    #The reply_list may be empty
    if(len(reply_list)==0):
        lock.release()
        return 0
    else:
        #Use the FCFS protocol to return replies to client application 
        reply=reply_list[0]
        del reply_list[0]
        lock.release()
        return reply


#getRequest function used from server application to get a request
def getRequest():
    global lock
    lock.acquire()
    #The receiver_list may be empty
    if(len(receiver_list)==0):
        lock.release()
        return 0
    else:
        #Give the requests to the server using the FCFS(First Come First Served) protocol
        #Do not delete the request from the list
        request=[receiver_list[0].get_number(),receiver_list[0].get_unique_id()]
        lock.release()
        return request



#After serving a request,server application calls this function to send the reply to the client middleware
def sendReply(reply_message,unique_id):
    #Create a UDP socket
    global reply_sequence_num
    global my_server_id
    
    #Find the request with id=unique_id and set the address to client reply ip and client reply address
    for i in range(0,len(receiver_list)):
        if(receiver_list[i].get_unique_id()==unique_id):
            address=(receiver_list[i].get_reply_ip(),receiver_list[i].get_reply_port())
            break
    #Delete the request from the list
    del receiver_list[i]
    sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    sock.settimeout(5.0)
    reply_sequence_num +=1
    reply=reply_message_class(reply_message,my_ip,my_port,reply_sequence_num,my_server_id)
    reply=pickle.dumps(reply)
    ackmsg=ack_message_class(0)
    
    #Send the reply to the client.Use the stop&wait protocol for retransmition
    #If the client is down,we use "at most once" semantics,so the middleware tries for MAX_RETRANSMMISIONS times to resend the reply message
    for i in range(0,MAX_RETRANSMMISIONS):
        try:
            sock.sendto(reply,address)
            data, addr = sock.recvfrom(1024)
        except socket.timeout:
            print("Got a timeout,resending")
            continue

        ackmsg=pickle.loads(data)
        #print("Got an ack message for message with sequence",ackmsg.get_sequence())
        if (ackmsg.get_sequence()==reply_sequence_num):
            break
        
        break
#Each server must register before he start serving requests
def register(svcid,port):
    global my_service_id
    global my_ip
    global my_port
    global my_server_id
    
    hostname = socket.gethostname()  
    IPAddr = socket.gethostbyname(hostname)    
    print("Your Computer Name is:" + hostname)    
    print("Your Computer IP Address is:" + IPAddr)
    #Initialize server's service id,server's ip and server's port
    my_service_id=svcid
    my_ip=IPAddr
    my_port=port
    my_server_id=random.random()
    thread1 = threading.Thread(target=receiver,args=())
    thread1.start()
    thread2 = threading.Thread(target=multicast_receiver,args=())
    thread2.start()
