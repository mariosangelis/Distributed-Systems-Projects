#Distributed systems application project
#Author : Angelis Marios
#NFS (Network File System) implementation using python
#We use exactly once semantics and a stop-and-wait networking protocol
import socket
from socket import AF_INET, SOCK_DGRAM
import pickle
import threading
import time
import random
import struct
from random import seed
from random import randrange
from random import randint
import uuid
import os

lock = threading.Lock()
nfs_list=[]
cache_list=[]
server_address,start_time,sequence_num,my_personal_id,increase_num=0,0,0,0,0
BLOCK_SIZE=1024
    
#This is the nfs_node object. The nfs_list contains nfs_node objects.
class nfs_node:
    
    def __init__(self,filename,fd_client,fd_apl,flags,file_size,t_check,t_mod):
        self.filename=filename;
        self.fd_client=fd_client
        self.fd_apl=fd_apl
        self.flags=flags
        self.file_size=file_size
        self.t_check=t_check
        self.t_mod=t_mod
        self.pos=0
        
    def get_fd_client(self):
        return self.fd_client
    
    def get_fd_apl(self):
        return self.fd_apl
        
    def get_filename(self):
        return self.filename
    
    def get_flags(self):
        return self.flags
    
    def get_tmod(self):
        return self.t_mod
    
    def get_file_size(self):
        return self.file_size
    
    def get_tcheck(self):
        return self.t_check
        
    def set_tcheck(self,new_tcheck):
        self.t_check=new_tcheck
        
    def set_fd_client(self,new_fd_client):
        self.fd_client=new_fd_client
        
    def set_filesize(self,new_file_size):
        self.file_size = new_file_size
        
    def set_tmod(self,new_tmod):
        self.t_mod=new_tmod
    
    def get_pos(self):
        return self.pos
    
    def set_pos(self,newpos):
        self.pos=newpos
        
    def set_size(self,new_size):
        self.size=new_size

#This is the nfs_message object used for the RPC client-server communication
class nfs_message:
    
    def __init__(self,filename,flags,message_type,seq,client_id,fd_client,pos,data,num_of_bytes,ls):
        self.filename=filename
        self.message_type=message_type
        self.flags=flags
        self.unique_id=0
        self.sequence=seq
        self.client_id=client_id
        self.fd_client=fd_client
        self.position=pos
        self.data=data
        self.ls=ls
        self.num_of_bytes=num_of_bytes

    def get_filename(self):
        return self.filename
    
    def get_ls(self):
        return self.ls
    
    def get_num_of_bytes(self):
        return self.num_of_bytes
    
    def get_data(self):
        return self.data
    
    def get_fd_client(self):
        return self.fd_client
    
    def get_pos(self):
        return self.position
    
    def get_flags(self):
        return self.flags
    
    def get_sequence(self):
        return self.sequence
    
    def get_message_type(self):
        return self.message_type
    
    def set_ls(self,val):
        self.ls=val
    
    def set_unique_id(self,newid):
        self.unique_id=newid
        
    def get_unique_id(self):
        return self.unique_id
    
    def get_client_id(self):
        return self.client_id
    
    
#This is the nfs_reply_message object used for the RPC client-server communication
class nfs_reply_message:
    
    def __init__(self,file_size,fd_client,tmod,sequence,data,size):
        self.file_size=file_size
        self.fd_client=fd_client
        self.tmod=tmod
        self.sequence=sequence
        self.unique_id=0
        self.data=data
        self.size=size

    def get_file_size(self):
        return self.file_size
    
    def get_size(self):
        return self.size
    
    def get_data(self):
        return self.data
    
    def get_fd_client(self):
        return self.fd_client
    
    def get_tmod(self):
        return self.tmod
    
    def get_sequence(self):
        return self.sequence
    
    
#This is the cache_node object. The cache_list contains cache_node objects.
class cache_node:
    def __init__(self,filename,start_pos,end_pos):
        self.filename=filename
        self.start_pos=start_pos
        self.end_pos=end_pos
        self.LRU=-1
        self.data=''
        
    def get_filename(self):
        return self.filename
    
    def get_data(self,start,end):
        return self.data[start:end]
        
    def get_start_pos(self):
        return self.start_pos
    
    def get_end_pos(self):
        return self.end_pos
    
    def get_LRU(self):
        return self.LRU
    
    def set_start_pos(self,new_pos):
        self.start_pos=new_pos
        
    def set_end_pos(self,new_end_pos):
        self.end_pos=new_end_pos
        
    def set_LRU(self,val):
        self.LRU=val
        
    def set_data(self,new_data):
        self.data=new_data
        
    def set_filename(self,filename):
        self.filename=filename
        
#This function sets the server address vector and initializes the client's personal id.
def mynfs_set_srv_addr(ip,port):
    global server_address
    global start_time
    global my_personal_id
    
    start_time=time.time()
    print(uuid.uuid4())
    my_personal_id = uuid.uuid4()
    print("id is ",my_personal_id)
    server_address = (ip,port)
    print("server address is ",server_address)
    
#This function initializes the software defined cache. We create cache_size/BLOCK_SIZE cache blocks.
#Also, this function initializes the freshness_time.
def mynfs_set_cache(cache_size,freshness_time):
    global freshness;
    global BLOCK_SIZE
    
    freshness=freshness_time
    for i in range(0,int(cache_size/BLOCK_SIZE)):
        newnode=cache_node(None,-1,-1)
        cache_list.append(newnode)
    
#This function updates the tcheck time interval for all the nodes of the nfs_list which have the same filename as the first argument.
def update_tcheck(filename,new_tcheck):
    for i in range (0,len(nfs_list)):
        if(nfs_list[i].get_filename()==filename):
            nfs_list[i].set_tcheck(new_tcheck)    

#This function updates the tmod for all the nodes of the nfs_list which have the same filename as the first argument.
def update_tmod(filename,new_tmod,refreshed_size):
    
    for i in range (0,len(nfs_list)):
        if(nfs_list[i].get_filename()==filename):
            nfs_list[i].set_filesize(refreshed_size)
            nfs_list[i].set_tmod(new_tmod)
    
#This function updates the client file descriptor for all the nodes of the nfs_list which have the same fd_client as the first argument.
def update_fd_client(fd_client,fd_client2):
    for i in range (0,len(nfs_list)):
        if(nfs_list[i].get_fd_client()==fd_client):
            nfs_list[i].set_fd_client(fd_client2)
    
#This function updates the position for the node which has the same application file descriptor as the first argument.
def update_pos(fd_apl,new_pos):
    for i in range (0,len(nfs_list)):
        if(nfs_list[i].get_fd_apl() == fd_apl):
            nfs_list[i].set_pos(new_pos)
            break    

#This function updates the file size variable for all the nodes of the nfs_list which have the same fd_client as the first argument.
def update_size(fd_client,size):
    for i in range (0,len(nfs_list)):
        if(nfs_list[i].get_fd_client()==fd_client):
            nfs_list[i].set_size(size)

#This function clears all cache nodes which have the same filename as the first argument.
def clear_cache(filename):
    for i in range (0,len(cache_list)):
        if(cache_list[i].get_filename()==filename):
            cache_list[i].set_LRU(-1)
            cache_list[i].set_start_pos(-1)
            cache_list[i].set_end_pos(-1)
            
#Fill a cache block with the fetched_data.
def update_cache(filename,start,end,fetched_data,fetched_bytes):
    
    #Search for an empty cache block.
    for i in range (0,len(cache_list)):
        if(cache_list[i].get_LRU()==-1):
            cache_list[i].set_LRU(0)
            cache_list[i].set_start_pos(start)
            cache_list[i].set_end_pos(end)
            cache_list[i].set_data(fetched_data[:])
            cache_list[i].set_filename(filename)
            return
            
    max_lru=-1
    replace_block=-1
    
    #Cache is full, replace a block via LRU method.
    for i in range (0,len(cache_list)):
        if(cache_list[i].get_LRU() > max_lru):
            max_lru = cache_list[i].get_LRU()
            replace_block = i
    
    cache_list[replace_block].set_start_pos(start)
    cache_list[replace_block].set_end_pos(end)
    cache_list[replace_block].set_data(fetched_data[:])
    cache_list[replace_block].set_filename(filename)
    return
    
#This function increases the LRU variable of all cache blocks except the empty ones and the block specified by the first argument.
def update_LRU(block):

    for i in range (0,len(cache_list)):
        if(i!=block and cache_list[i].get_LRU!=-1):
            cache_list[i].set_LRU(cache_list[i].get_LRU()+1)

    cache_list[block].set_LRU(0)
    
#This is the openRPC function
def openRPC(filename,flags,ls_flag):
    
    global server_address
    global sequence_num
    global my_personal_id
    
    #Create a UDP socket
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    sock.settimeout(5.0)
    sequence_num +=1
    nfs_msg=nfs_message(filename,flags,"OPEN",sequence_num,my_personal_id,0,0,0,0,ls_flag)
    nfs_msg=pickle.dumps(nfs_msg)
    reply=nfs_reply_message(0,0,0,0,0,0)
    
    print(server_address)
    
    #Send the nfs_message to the server
    #Use the stop&wait protocol for retransmition
    while(1):
        try:
            sock.sendto(nfs_msg,server_address)
            data, addr = sock.recvfrom(1024)
        except socket.timeout:
            print("Got a timeout,resending")
            continue
        
        reply=pickle.loads(data)
        if (reply.get_sequence()==sequence_num):
            break
    
    #mynfs_ls function called this openRPC. Return a list of the files included in the server's directory.
    if(ls_flag==1):
        return reply.get_data()
    return [reply.get_fd_client(),reply.get_file_size(),reply.get_tmod()]
    
#This is the readRPC function
def readRPC(filename,fd_client,pos):
    
    global server_address
    global sequence_num
    global my_personal_id
    
    #Create a UDP socket
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    sock.settimeout(5.0)
    sequence_num +=1 
    nfs_msg=nfs_message(filename,0,"READ",sequence_num,my_personal_id,fd_client,pos,0,0,0)
    nfs_msg=pickle.dumps(nfs_msg)
    reply=nfs_reply_message(0,0,0,0,0,0)
    
    #Send the nfs_message to the server
    #Use the stop&wait protocol for retransmition
    while(1):
        try:
            sock.sendto(nfs_msg,server_address)
            data, addr = sock.recvfrom(3000)
        except socket.timeout:
            print("Got a timeout,resending")
            continue
        
        reply=pickle.loads(data)
        if (reply.get_sequence()==sequence_num):
            break
    
    #Return the fetched data, the number of fetched bytes and the possibly updated tmod.
    return [reply.get_data(),reply.get_size(),reply.get_tmod()]

#This is the writeRPC function
def writeRPC(filename,fd_client,data,num_of_bytes,pos):
    
    global server_address
    global sequence_num
    global my_personal_id
    
    #Create a UDP socket
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    sock.settimeout(5.0)
    sequence_num +=1 
    nfs_msg=nfs_message(filename,0,"WRITE",sequence_num,my_personal_id,fd_client,pos,data,num_of_bytes,0)
    nfs_msg=pickle.dumps(nfs_msg)
    reply=nfs_reply_message(0,0,0,0,0,0)
    
    #Send the nfs_message to the server
    #Use the stop&wait protocol for retransmition
    while(1):
        try:
            sock.sendto(nfs_msg,server_address)
            data, addr = sock.recvfrom(3000)
        except socket.timeout:
            print("Got a timeout,resending")
            continue
        
        reply=pickle.loads(data)
        if (reply.get_sequence()==sequence_num):
            break
        
    if(reply.get_size()!=-1):
        #Update tmod and clear the cache.
        update_tmod(filename,reply.get_tmod(),reply.get_file_size())
        clear_cache(filename)
        
    #Return the fetched data, the number of fetched bytes and the possibly updated tmod.
    return reply.get_size()
    
    
#This function updates the position of the nfs_node which has the same application file descriptor as the first argument.
def mynfs_lseek(fd_appl,offset,whence):
    
    for i in range (0,len(nfs_list)):
        if(nfs_list[i].get_fd_apl()==fd_appl):
            
            if(whence==os.SEEK_SET):
                nfs_list[i].set_pos(offset)
            elif(whence==os.SEEK_CUR):
                nfs_list[i].set_pos(nfs_list[i].get_pos()+offset)
            elif(whence==os.SEEK_END):
                nfs_list[i].set_pos(nfs_list[i].get_file_size()+offset)
            return
    
#This is the mynfs_open function 
def mynfs_open(filename,flags):
    
    fd_client,file_size,tmod,found_same_file,howmany=-1,0,0,0,0
    global increase_num
    
    #Search for an nfs_node with the same filename and the same flags
    for i in range (0,len(nfs_list)):
        if(nfs_list[i].get_filename() == filename and nfs_list[i].get_flags()==flags):
            fd_client = nfs_list[i].get_fd_client()
            file_size = nfs_list[i].get_file_size()
            tmod = nfs_list[i].get_tmod()
            found_same_file=1
            break
        
    if(found_same_file==0 or flags==os.O_RDONLY|os.O_TRUNC or flags==os.O_WRONLY|os.O_TRUNC or flags==os.O_RDWR|os.O_CREAT|os.O_TRUNC):
        #If the "flags" variable contains the os.O_TRUNC mode or if there is not n nfs_node with the same filename and the same flags, call the openRPC function.
        fd_client,file_size,tmod=openRPC(filename,flags,0)
        if(fd_client==-1):
            #If an error has occured in the server's open function,return -1 to the application.
            return -1
        
        howmany=0
        for i in range (0,len(nfs_list)):
            if(nfs_list[i].get_filename() == filename):
                howmany+=1
        #If there are other nfs_nodes with the same filename and different file descriptor, update their tmod.
        #tmod may has changed because of the os.O_TRUNC flag.
        if(howmany > 0):
            update_tmod(filename,tmod,file_size)
        
        #Update tcheck time interval, because of the current communication with the server.
        update_tcheck(filename,int(time.time()-start_time))
        
    #Create an application file descriptor and return it to the application.
    increase_num+=1
    fd_appl = fd_client + increase_num
    print("Return to application fd : ",fd_appl)
    
    #Create a new nfs_node and append it into the nfs_list.
    newnode = nfs_node(filename,fd_client,fd_appl,flags,file_size,int(time.time()-start_time),tmod)
    nfs_list.append(newnode)
    
    return fd_appl
    
#This is the mynfs_read function
def mynfs_read(fd_apl,num_of_bytes):
    
    global freshness
    global BLOCK_SIZE
    found_same_file,fd_client,file_size,tmod,tcheck,filename,pos,cache_found,final_bytes,flags=0,0,0,0,0,0,0,0,0,0
    data=bytearray()
    
    #If the length of the data is bigger than a cache page, read only a cache page from the server.
    if(num_of_bytes>BLOCK_SIZE):
        num_of_bytes=BLOCK_SIZE
    final_bytes=num_of_bytes
    
    for i in range (0,len(nfs_list)):
        if(nfs_list[i].get_fd_apl() == fd_apl):
            filename=nfs_list[i].get_filename()
            flags=nfs_list[i].get_flags()
            fd_client = nfs_list[i].get_fd_client()
            file_size = nfs_list[i].get_file_size()
            tmod = nfs_list[i].get_tmod()
            tcheck=nfs_list[i].get_tcheck()
            pos=nfs_list[i].get_pos()
            found_same_file=1
            break
        
        
    if(found_same_file==0):
        return [-1,-1]
    
    #Check the cache data validity
    if((int(time.time()-start_time) - tcheck) > freshness):
        print("Freshness has expired,check tmod")
        
        fd_client2,refreshed_size,new_tmod=openRPC(filename,flags,0)
        #The server may has deleted the nfs_server_node due to low usability.
        #In this case, the openRPC call will return a new file descriptor.
        if(fd_client2!=fd_client):
            #fd_client has changed
            update_fd_client(fd_client,fd_client2)
            fd_client=fd_client2
        
        print("Got from server:tmod=",new_tmod,"size=",refreshed_size)
        #Update tcheck time interval, because of the current communication with the server.
        update_tcheck(filename,int(time.time()-start_time))
        
        if(new_tmod != tmod):
            #The file has changed, so the openRPC function returned a new tmod.
            #Update tmod and clear the cache.
            update_tmod(filename,new_tmod,refreshed_size)
            clear_cache(filename)
        
        
    start=0
    while(1):
    
        for i in range (0,len(cache_list)):
            if(cache_list[i].get_filename() == filename):
                #All data are located inside this cache page
                if(pos >= cache_list[i].get_start_pos() and pos < cache_list[i].get_end_pos() and (pos+num_of_bytes-1) <= cache_list[i].get_end_pos()):
                    #This is a cache hit.
                    update_LRU(i)
                    data += bytes(cache_list[i].get_data(pos- cache_list[i].get_start_pos(),pos- cache_list[i].get_start_pos() + num_of_bytes))
                    update_pos(fd_apl,pos+num_of_bytes)
                    return [data,final_bytes]
                
                elif(pos >= cache_list[i].get_start_pos() and pos < cache_list[i].get_end_pos() and (pos+num_of_bytes-1) > cache_list[i].get_end_pos()):
                    #Only a part of the data are located inside this cache page.
                    data += bytes(cache_list[i].get_data(pos- cache_list[i].get_start_pos(),cache_list[i].get_end_pos() - cache_list[i].get_start_pos() + 1))
                    update_LRU(i)
                    pos += cache_list[i].get_end_pos() - pos + 1
                    num_of_bytes -= cache_list[i].get_end_pos() - pos + 1
                    start+= cache_list[i].get_end_pos() - pos + 1
                    cache_found+=1;
                    
                    
        if(cache_found==0):
            #Dont found data in anyone cache page,so fetch them from the server.
            fetched_data,fetched_bytes,new_tmod=readRPC(filename,fd_client,pos)
            if(fetched_bytes==-1):
                #If an error has occured in the server's read function,return -1 to application
                return [-1,-1]

            if(fetched_bytes==0):
                print("reached EOF")
                return [data,final_bytes-num_of_bytes]
            
            update_cache(filename,int(pos/BLOCK_SIZE)*BLOCK_SIZE,int(pos/BLOCK_SIZE)*BLOCK_SIZE+fetched_bytes-1,fetched_data,fetched_bytes)          
        else:
            cache_found=0
            
            
#This is the mynfs_write function
def mynfs_write(fd_apl,data,num_of_bytes):
    
    global freshness
    global BLOCK_SIZE
    found_same_file,fd_client,file_size,tmod,tcheck,filename,pos,cache_found,final_bytes,flags=0,0,0,0,0,0,0,0,0,0
    
    
    for i in range (0,len(nfs_list)):
        if(nfs_list[i].get_fd_apl() == fd_apl):
            filename=nfs_list[i].get_filename()
            flags=nfs_list[i].get_flags()
            fd_client = nfs_list[i].get_fd_client()
            file_size = nfs_list[i].get_file_size()
            tmod = nfs_list[i].get_tmod()
            tcheck=nfs_list[i].get_tcheck()
            pos=nfs_list[i].get_pos()
            found_same_file=1
            break
            
    
    n=num_of_bytes
    start=0
    
    #If the length of the data is bigger than a cache page,write the data "page to page" to the server
    while(n>BLOCK_SIZE):
        
        ret=writeRPC(filename,fd_client,data[start:start+BLOCK_SIZE],BLOCK_SIZE,pos)
        if(ret==-1):
            #The server may has deleted the nfs_server_node due to low usability.
            #In this case, the openRPC call will return a new file descriptor.
            fd_client2,refreshed_size,new_tmod=openRPC(filename,flags,0)
            if(fd_client2!=fd_client):
                #fd_client has changed
                update_fd_client(fd_client,fd_client2)
                fd_client=fd_client2
                update_size(fd_client,refreshed_size)
            
            print("Got from server:tmod=",new_tmod,"size=",refreshed_size)
            #Make again the writeRPC call.
            ret=writeRPC(filename,fd_client,data[start:start+BLOCK_SIZE],BLOCK_SIZE,pos)
        
        #Update tcheck time interval, because of the current communication with the server.
        update_tcheck(filename,int(time.time()-start_time))
        update_pos(fd_apl,pos+ret)
        pos+=ret
        
        n-=BLOCK_SIZE
        start+=BLOCK_SIZE
    
    ret=writeRPC(filename,fd_client,data[start:start+n],n,pos)
    if(ret==-1):
        #The server may has deleted the nfs_server_node due to low usability.
        #In this case, the openRPC call will return a new file descriptor.
        fd_client2,refreshed_size,new_tmod=openRPC(filename,flags,0)
        if(fd_client2!=fd_client):
            #fd_client has changed
            update_fd_client(fd_client,fd_client2)
            fd_client=fd_client2
            update_size(fd_client,refreshed_size)
            
        print("Got from server:tmod=",new_tmod,"size=",refreshed_size)
        #Make again the writeRPC call.
        ret=writeRPC(filename,fd_client,data[start:start+BLOCK_SIZE],BLOCK_SIZE,pos)
        
    #Update tcheck time interval, because of the current communication with the server.
    update_tcheck(filename,int(time.time()-start_time))
    update_pos(fd_apl,pos+ret)
    
    return num_of_bytes
            
#This function deletes from the nfs_list the node which has the same application file descriptor as the argument.
def mynfs_close(fd_apl):
    for j in range (0,len(nfs_list)):
        if(nfs_list[j].get_fd_apl() == fd_apl):
            print("close",fd_apl)
            del nfs_list[j]
            break
            
            
#This is the mynfs_ls function. Set the ls flag to 1 and call the openRPC function             
def mynfs_ls(path):
    #ls flag is 1
    data=openRPC(path,0,1)
    return data
    
    
    
    
    
    
    
    

