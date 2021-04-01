#Distributed systems application project
#Author : Angelis Marios
#Asynchronous request-reply model with semantics at least once and load balancing service.
#We use a pipeline sending model in order to send the requests from client to server. 
#Also, we used the advertised window model to not overflow the server buffer.

import socket
from socket import AF_INET, SOCK_DGRAM
import pickle
import threading
import time
import random
import struct
lock = threading.Lock()
receiver_list=[]
message_list=[]
sequence_number_list=[]
MAX_RETRANSMMISIONS=5
MAX_SERVER_ADVERTISED_WINDOW=100
GARBAGE_COLLECTOR_START_TIME=30
PIPELINE_RESENDER_START_TIME=20
SERVER_POLL_START_TIME=20
MULTICAST_PORT=10000
first,my_service_id,my_ip,my_port,my_reply_ip,my_reply_port,sequence_number,my_private_id,my_server_id,server_address=0,0,0,0,0,0,0,0,0,0
base,exp_reply_seq_num=1,1
AdvertisedWindow=MAX_SERVER_ADVERTISED_WINDOW
MULTICAST_ADDRESS="224.0.0.1"

class message_class:
    
    def __init__(self,svc_id,seq,number,reply_ip,reply_port,client_id):
        self.sequence = seq
        self.number=number
        self.reply_ip=reply_ip
        self.reply_port=reply_port
        self.client_id=client_id
        self.unique_id=0
        self.service_id=svc_id
        
    def get_sequence(self):
        return(self.sequence)
    
    def get_service_id(self):
        return(self.service_id)
    
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
        

class message_node:
    
    def __init__(self,message):
        self.ack_flag=0;
        self.read_flag=0;
        self.message=message;
        self.reply=None
    
    def isAcked(self):
        return self.ack_flag;
    
    def isRead(self):
        return self.read_flag;
    
    def getMessage(self):
        return self.message
    
    def setAckFlag(self):
        self.ack_flag=1;
        
    def setReadFlag(self):
        self.read_flag=1;
    
    def setReply(self,new_reply):
        self.reply=new_reply;
        
    def getReply(self):
        return(self.reply);
    

class reply_message_class:
    
    def __init__(self, message,port,ip,seq,server_id,request_seq_num):
        self.message = message
        self.port=port
        self.ip=ip
        self.sequence=seq
        self.server_id=server_id
        self.request_seq_num=request_seq_num
        self.isAckMsg=0
        self.PollFlag=0;
        self.AdvertisedWindow=MAX_SERVER_ADVERTISED_WINDOW
        
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
    
    def get_request_seq_num(self):
        return(self.request_seq_num)
    
    def isAckMessage(self):
        return self.isAckMsg
    
    def setAck(self):
        self.isAckMsg=1
        
    def getAdvertisedWindow(self):
        return self.AdvertisedWindow
    
    def setAdvertisedWindow(self,new_size):
        self.AdvertisedWindow=new_size
        
    def setPollFlag(self):
        self.PollFlag=1
    
    def getPollFlag(self):
        return self.PollFlag
    
#sendRequest function used from client to send requests
def sendRequest(number,svcid,reply_port):
    global first
    global my_service_id
    global MULTICAST_PORT
    global MULTICAST_ADDRESS
    global my_reply_ip
    global my_reply_port
    global sequence_number
    global my_private_id
    global base
    global lock
    global server_address
    global AdvertisedWindow

    
    flag=0
    min_capacity=100000
    
    #Initialize service_id,reply_ip,reply_port and start the reply_receiver,garbage_collector and pipeline_resender threads.
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
        
        discovery_message = message_class(my_service_id,0,0,my_reply_ip,reply_port,my_private_id)
        multicast_group = (MULTICAST_ADDRESS, MULTICAST_PORT)

        #Create a  multicast socket
        multicast_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        multicast_sock.settimeout(0.5)
        ttl = struct.pack('b', 1)
        multicast_sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl)
        
        #Send a discovery message to the multicast channel.
        discovery_message=pickle.dumps(discovery_message)
        multicast_sock.sendto(discovery_message, multicast_group)
        
        #Wait to receive a reply for the discovery message from each server which serves the same service id.
        while(1):
            discovery_reply=discovery_reply_class(0,0,0,0)
            try:
                data, addr = multicast_sock.recvfrom(1024)
            except socket.timeout:
                #All servers sent a discovery reply message.
                break
            flag=1
            discovery_reply=pickle.loads(data)
            
            #Find the server with the minimum capacity.
            if(discovery_reply.get_capacity()<min_capacity):
                min_capacity=discovery_reply.get_capacity()
                server_address=(discovery_reply.get_ip(),discovery_reply.get_port())
        
            if(flag==0):
                #None server is available,clear servers list.
                return -1
        
        print("Server address is ",server_address)
        
        #Create and start threads
        thread = threading.Thread(target=reply_receiver,args=())
        thread1 = threading.Thread(target=garbage_collector,args=())
        thread2 = threading.Thread(target=pipeline_resender,args=())
        
        thread.start()
        thread1.start()
        thread2.start()
    
    
    sequence_number +=1
    #Create the request message and add it to the message_list.
    send_message=message_class(my_service_id,sequence_number,number,my_reply_ip,reply_port,my_private_id)
    new_node=message_node(send_message)
    lock.acquire()
    message_list.append(new_node);
    lock.release()
    
    while(1):
        lock.acquire()
        #Send the request only if the total number of received and not acked messages are less than the server's advertised window.
        if((sequence_number-base) < AdvertisedWindow and AdvertisedWindow>0):
            #Create a UDP socket
            sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
            sock.settimeout(5.0)
            send_message=pickle.dumps(send_message)
            sock.sendto(send_message,server_address)
            
            lock.release()
            break

        lock.release()
        #Can not send the request due to big congestion on the server side. Try again after sleeping 2 seconds.
        #print("Spinning")
        time.sleep(2);
   
#Receiver thread is used from server to receive requests
def receiver():
    
    global lock
    
    i=0
    #Creates a udp socket and bind to "listen" requests from this channel.
    address=(my_ip,my_port)
    sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(address)
    
    
    while 1:
        flag=0
        rcvmsg=message_class(0,0,0,0,0,0)
        data, addr = sock.recvfrom(1024)
        rcvmsg=pickle.loads(data)
        
        #Duplicate packets check
        #We use a list(each index contains a list) of lists(each index contains the sequence_number of the most recently read message for each client and the reply sequence number used
        #for the replie messages to this client).
        #ex : This server serves 2 clients.First client has id:2.19234 and second client has id:5.12352.The most recently read message from the first client is a message with sequence_number=2 and the most recently read message from the second client is a message with sequence_number=6.The sequence_number_list will be as follows :
        #[[2.19234,2,1],[5.12352,6,1]].This means that the server is waiting for a message from client 1 with sequence_number larger than 2 or a message from client 2 with sequence_number larger than 6.
        
        lock.acquire()
        #Get exp_seq_num variable for this client.
        for i in range(0,len(sequence_number_list)):
            if(rcvmsg.get_client_id()==sequence_number_list[i][0]):
                exp_seq_num=sequence_number_list[i][1]
                
            
        #The previous ACK message maybe has not arrived to the client,so this request is duplicate. Resend the ACK message.
        #ex:sequence_number of this message is 4 and exp_seq_num is 8, so resend the ACK message.
        #Inside the ack message, include the receiver list length, in order to control buffer overflow.
        if(rcvmsg.get_sequence() < exp_seq_num):
            addr=(rcvmsg.get_reply_ip(),rcvmsg.get_reply_port())
            message=reply_message_class(0,0,0,rcvmsg.get_sequence(),0,0)
            message.setAdvertisedWindow(MAX_SERVER_ADVERTISED_WINDOW-len(receiver_list))
            message.setAck()
            message=pickle.dumps(message)
            sock.sendto(message,addr)
        else:
            #ex:sequence_number of this message is 4 and exp_seq_num is 4,so this is a new message.
            if(rcvmsg.get_sequence() == exp_seq_num):
                
                #Search inside receiver_list to find the correct value of the exp_seq_num variable.
                #ex: At first, suppose that server receives messages with sequence numbers=1,2,3,5,6,7. Later, the server receives a message with sequence number=4.
                #The correct value of exp_seq_num is 8.
                for j in range(0,len(receiver_list)):
                    for i in range(0,len(receiver_list)):
                        if(rcvmsg.get_client_id()==receiver_list[i].get_client_id()):
                            if(receiver_list[i].get_sequence()==exp_seq_num+1):
                                exp_seq_num+=1
                            
                exp_seq_num+=1
                
                #Refresh the exp_seq_num variable into sequence_number_list.
                for i in range(0,len(sequence_number_list)):
                    if(rcvmsg.get_client_id()==sequence_number_list[i][0]):
                        sequence_number_list[i][1]=exp_seq_num
                        break
                    
            
            #Add the request to the receiver list.Give a unique id in each request.
            rcvmsg.set_unique_id(random.random())
            receiver_list.append(rcvmsg)
            print("Got a request for number ",rcvmsg.get_number(),"with seq_id ",rcvmsg.get_sequence(),"reply to ip ",rcvmsg.get_reply_ip(),"and to port ",rcvmsg.get_reply_port())
            #Send an ACK message back to sender.
            addr=(rcvmsg.get_reply_ip(),rcvmsg.get_reply_port())
            message=reply_message_class(0,0,0,rcvmsg.get_sequence(),0,0)
            message.setAck()
            message.setAdvertisedWindow(MAX_SERVER_ADVERTISED_WINDOW-len(receiver_list))
            message=pickle.dumps(message)
            sock.sendto(message,addr)

        lock.release()
   
#multicast_receiver thread used from server to receive multicast messages
def multicast_receiver():
    global lock
    global MULTICAST_PORT
    global MULTICAST_ADDRESS
    global sequence_number_list
    
    #Create a multicast socket and bind to the multicast address in order to "listen" discovery messages.
    multicast_address=(MULTICAST_ADDRESS,MULTICAST_PORT)
    sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(multicast_address)
    i=0
    
    group = socket.inet_aton(MULTICAST_ADDRESS)
    mreq = struct.pack('4sL', group, socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    
    while 1:
        flag=0
        rcvmsg=message_class(0,0,0,0,0,0)
        data, addr = sock.recvfrom(1024)
        rcvmsg=pickle.loads(data)
        #Send a point to point discovery reply message only if the server can serve this request.
        if(rcvmsg.get_service_id()==my_service_id):
            lock.acquire()
            for i in range(0,len(sequence_number_list)):
                if(rcvmsg.get_client_id()==sequence_number_list[i][0]):
                    flag=1
                    break
                    
            if(flag==0):
                print("New client,add his client_id to the sequence_number_list")
                #If the message has sequence_number=5,add the list [client_id of this client,4,client_ip,client_port,0] to the sequence_number_list.
                #The last 0 is called delete flag and it is used from the polling thread.
                sequence_number_list.append([rcvmsg.get_client_id(),1,1,rcvmsg.get_reply_ip(),rcvmsg.get_reply_port(),0])

            #Send back to client a discovery reply message which contains the server's capacity,his ip and his port.
            discovery_reply_message=discovery_reply_class(my_service_id,len(receiver_list),my_ip,my_port)
            lock.release()
            discovery_reply_message=pickle.dumps(discovery_reply_message)
            sock.sendto(discovery_reply_message,addr)
    

#getRequest function used from server in order to get a request
def getRequest():
    global lock
    lock.acquire()
    #The receiver_list may be empty.
    if(len(receiver_list)==0):
        lock.release()
        return 0
    else:
        #Give the requests to the server using the FCFS(First Come First Served) protocol.
        #Do not delete the request from the list.
        request=[receiver_list[0].get_number(),receiver_list[0].get_unique_id()]
        lock.release()
        return request

#After serving a request,server application calls this function to send the reply to the client middleware
def sendReply(reply_message,unique_id):
    
    global my_server_id
    global lock
    
    #Find the request with id=unique_id and set the address to (client reply ip,client reply address).
    #Extract the request sequence number and attach it inside the reply message.
    #The client will match the reply with the request by checking their sequence numbers.
    for i in range(0,len(receiver_list)):
        if(receiver_list[i].get_unique_id()==unique_id):
            address=(receiver_list[i].get_reply_ip(),receiver_list[i].get_reply_port())
            request_seq_num=receiver_list[i].get_sequence();
            client_id=receiver_list[i].get_client_id()
            break
        
    #Delete the request from receiver_list.
    del receiver_list[i]
    sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    sock.settimeout(5.0)
    
    lock.acquire()
    #Refresh request_seq_num variable for this client.
    for i in range(0,len(sequence_number_list)):
        if(client_id==sequence_number_list[i][0]):
            reply_sequence_num=sequence_number_list[i][2]
            sequence_number_list[i][2]+=1
            break
    #Create the reply message.
    reply=reply_message_class(reply_message,my_ip,my_port,reply_sequence_num,my_server_id,request_seq_num)
    reply.setAdvertisedWindow(MAX_SERVER_ADVERTISED_WINDOW-len(receiver_list))
    reply=pickle.dumps(reply)
    ackmsg=reply_message_class(0,0,0,0,0,0)

    lock.release()
    
    #Send the reply to client.Use the stop&wait protocol for retransmition.
    #If the client is down,we use "at most once" semantics,so the middleware tries for MAX_RETRANSMMISIONS times to resend the reply message.
    for i in range(0,MAX_RETRANSMMISIONS):
        try:
            sock.sendto(reply,address)
            data, addr = sock.recvfrom(1024)
        except socket.timeout:
            print("Got a timeout,resending")
            continue

        ackmsg=pickle.loads(data)
        if (ackmsg.get_sequence()==reply_sequence_num):
            break
        
        break
 
 
#reply_receiver thread used from client to receive replies from server       
def reply_receiver():
    global lock
    global my_reply_ip
    global my_reply_port
    global exp_reply_seq_num
    global base
    global AdvertisedWindow
    
    flag=0
    #Create a UDP socket.
    address=(my_reply_ip,my_reply_port)
    sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(address)
    
    while 1:
        flag=0
        served_sequence=0
        rcvmsg=reply_message_class(0,0,0,0,0,0)
        data, addr = sock.recvfrom(1024)
        rcvmsg=pickle.loads(data)   
        
        #Refresh the AdvertisedWindow variable.
        AdvertisedWindow=rcvmsg.getAdvertisedWindow()
        
        #This is a poll message from server
        if(rcvmsg.getPollFlag()==1):
            message=reply_message_class(0,0,0,0,0,0)
            message.setAck()
            message=pickle.dumps(message)
            sock.sendto(message,addr)
        
        #This is an ack message.
        elif(rcvmsg.isAckMessage()):
            lock.acquire()
            
            #Set the ack flag after matching the request sequence number with the ack message sequence number.
            for i in range(0,len(message_list)):
                if(rcvmsg.get_sequence()==message_list[i].getMessage().get_sequence()):
                    message_list[i].setAckFlag()
                    break
            
            #Search inside message_list to find the correct value of the base variable.
            for j in range(0,len(message_list)):
                for i in range(0,len(message_list)):
                    if(message_list[i].getMessage().get_sequence()==base and message_list[i].isAcked()==1):
                        base+=1
            
            lock.release()
        
        else:
            #This is a reply message.
            #Duplicate packets check.
            #The previous REPLY ACK message has not arrived to the server,so this reply is duplicate. Resend the REPLY ACK message.
            #ex:sequence_number of this reply message is 4 and served_sequence is 8, so resend the REPLY ACK message.
            if(rcvmsg.get_sequence()<exp_reply_seq_num):
                message=ack_message_class(rcvmsg.get_sequence())
                message=pickle.dumps(message)
                sock.sendto(message,addr)
                
            else:
                lock.acquire()
                #ex:sequence_number of this reply message is 4 and exp_reply_seq_num is 4,so this is a new reply message.
                if(rcvmsg.get_sequence()==exp_reply_seq_num):              
                    exp_reply_seq_num+=1
                    
                #Set the reply.
                for i in range(0,len(message_list)):
                    if(rcvmsg.get_request_seq_num()==message_list[i].getMessage().get_sequence()):
                        message_list[i].setReply(rcvmsg.get_message());
                        break
                
                lock.release()
                #Send a reply ACK message back to server.
                message=reply_message_class(0,0,0,rcvmsg.get_sequence(),0,0)
                message.setAck()
                message=pickle.dumps(message)
                sock.sendto(message,addr)
 
 
#When the client application wants to get a reply for a request which was sent before,it calls the getReply function
def getReply():
    global lock
    
    lock.acquire()
    
    #Return the reply only if this message node has : ackFlag=1,ReadFlag=0 and reply!=None.
    for i in range(0,len(message_list)):
        if(message_list[i].isAcked()==1 and message_list[i].isRead()==0 and message_list[i].getReply()!=None):
            reply=message_list[i].getReply()
            message_list[i].setReadFlag()
            lock.release()
            return reply
    
    lock.release()
    return 0
    
#Garbage collector function
#This function deletes only the nodes from the message list which have : AckFlag=1 and ReadFlag=1.
def garbage_collector():
    global lock
    global GARBAGE_COLLECTOR_START_TIME
    
    while(1):
        time.sleep(GARBAGE_COLLECTOR_START_TIME)
        lock.acquire()
        
        print("******GARBAGE COLLECTOR ON******")
        for j in range(0,len(message_list)):
            for i in range(0,len(message_list)):
                if(message_list[i].isAcked()==1 and message_list[i].isRead()==1):
                    #print("Delete node with seqnum : ",message_list[i].getMessage().get_sequence())
                    del message_list[i] 
                    break
            
        print("******GARBAGE COLLECTOR OFF******")
        lock.release()

#Pipeline resender function
#This function resends only the nodes from the message list which have : AckFlag=0.
def pipeline_resender():
    
    global my_service_id
    global my_reply_ip
    global my_reply_port
    global my_private_id
    global lock
    global server_address
    global PIPELINE_RESENDER_START_TIME
    
    while(1):
        time.sleep(PIPELINE_RESENDER_START_TIME)
        lock.acquire()
        
        print("******PIPELINE RESENDER ON******")
        for i in range(0,len(message_list)):
            if(message_list[i].isAcked()==0):
                print("Resending message with sequence number ",message_list[i].getMessage().get_sequence())
                send_message=message_class(my_service_id,message_list[i].getMessage().get_sequence(),message_list[i].getMessage().get_number(),my_reply_ip,my_reply_port,my_private_id)
                #Create a UDP socket
                sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
                sock.settimeout(5.0)
                send_message=pickle.dumps(send_message)
                sock.sendto(send_message,server_address) 
            
        print("******PIPELINE RESENDER OFF******")
        lock.release()
        
def poll():
    global lock
    global SERVER_POLL_START_TIME
    
    while(1):
        time.sleep(SERVER_POLL_START_TIME)
        print("******POLLING THREAD ON******")
        lock.acquire()
        print("Len of sequence_number_list before polling is ",len(sequence_number_list))
        print("Length of receiver_list before polling is ",len(receiver_list))
        for i in range(0,len(sequence_number_list)):
            poll_message=reply_message_class("POLL",0,0,0,0,0)
            poll_message.setPollFlag()
            poll_message=pickle.dumps(poll_message)
                
            address=(sequence_number_list[i][3],int(sequence_number_list[i][4]))
                
            sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
            sock.settimeout(5.0)
            
            try:
                sock.sendto(poll_message,address)
                data, addr = sock.recvfrom(1024)
                print("Client is online, continue")
            except socket.timeout:
                print("Client is offline,delete all his requests")
                
                for k in range(0,len(receiver_list)):
                    for l in range(0,len(receiver_list)):
                        if(sequence_number_list[i][0]==receiver_list[l].get_client_id()):
                            del receiver_list[l]
                            break
                    
                #Set delete flag to 1
                sequence_number_list[i][5]=1;
                
        print("Length of receiver_list after polling is ",len(receiver_list))
        for j in range(0,len(sequence_number_list)):
            for i in range(0,len(sequence_number_list)):
                if(sequence_number_list[i][5]==1):
                    del sequence_number_list[i]
                    break
        print("Len of sequence_number_list after polling is ",len(sequence_number_list))
        
        lock.release()
        print("******POLLING THREAD OFF******")
    
    

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
    #Initialize server's service id,server's ip and server's port.
    my_service_id=svcid
    my_ip=IPAddr
    my_port=port
    my_server_id=random.random()
    thread1 = threading.Thread(target=receiver,args=())
    thread1.start()
    thread2 = threading.Thread(target=multicast_receiver,args=())
    thread2.start()
    thread3 = threading.Thread(target=poll,args=())
    thread3.start()
