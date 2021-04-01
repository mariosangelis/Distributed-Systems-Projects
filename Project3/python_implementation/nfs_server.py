#Distributed systems application project
#Author : Angelis Marios
#NFS (Network File System) implementation using python
#We use exactly once semantics and a stop-and-wait networking protocol
import time
import sys
import random
import os
import threading
from pathlib import Path
from nfs_client_middleware import *

sequence_number_list=[]
nfs_server_list=[]
lock = threading.Lock()
GARBAGE_COLLECTOR_TIME_INTERVAL=20
SERVER_POLL_START_TIME=20

#This is the nfs_server_node object. The nfs_server_list contains nfs_server_node objects.
class nfs_server_node:
    
    def __init__(self,fd_client,fd_OS,pos,tmod,filename,flags,active_time):
        self.fd_client = fd_client
        self.fd_OS=fd_OS
        self.pos=pos
        self.tmod=tmod
        self.flags=flags
        self.filename=filename
        self.active_time=active_time
        
    def set_tmod(self,new_tmod):
        self.tmod=new_tmod
        
    def get_fd_client(self):
        return(self.fd_client)
    
    def get_active_time(self):
        return(self.active_time)
    
    def set_active_time(self,new_active_time):
        self.active_time=new_active_time
    
    def get_fd_OS(self):
        return(self.fd_OS)
    
    def get_pos(self):
        return(self.pos)
    
    def get_tmod(self):
        return(self.tmod)
    
    def get_filename(self):
        return(self.filename)
    
    def get_flags(self):
        return(self.flags)
    

def update_tmod(filename):
    ret=0
    
    for i in range(0,len(nfs_server_list)):
        if(nfs_server_list[i].get_filename()==filename):
            nfs_server_list[i].set_tmod(nfs_server_list[i].get_tmod() + 1)
            ret=nfs_server_list[i].get_tmod()
            
    return ret

    
def main():
    
    global start_time
    global lock
    if(len(sys.argv)<3):
        print("Wrong number of arguments")
    else:
        
        start_time=time.time()
        increase_num=0
        address=(sys.argv[1],int(sys.argv[2]))
        sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(address)
        
        thread = threading.Thread(target=garbage_collector,args=())
        thread.start()
    
        while(1):
            flag=0
            rcvmsg=nfs_message(0,0,0,0,0,0,0,0,0,0)
            data, addr = sock.recvfrom(3000)
            rcvmsg=pickle.loads(data)
            lock.acquire()
            
            for i in range(0,len(sequence_number_list)):
                if(rcvmsg.get_client_id()==sequence_number_list[i][0]):
                    flag=1
                    exp_seq_num=sequence_number_list[i][1]
                    break
                    
            if(flag==0):
                print("New client,add his client_id to the sequence_number_list")
                print("client id is ",rcvmsg.get_client_id())
                #If the message has sequence_number=1,add the list [client_id of this client,1] to the sequence_number_list.
                sequence_number_list.append([rcvmsg.get_client_id(),1])
                exp_seq_num=1
                
            if(exp_seq_num==rcvmsg.get_sequence()):
                    
                for i in range(0,len(sequence_number_list)):
                    if(rcvmsg.get_client_id()==sequence_number_list[i][0]):
                        sequence_number_list[i][1]+=1
                        break
                
                if(rcvmsg.get_message_type()=="OPEN"):
                    #Received an openRPC message.
                    
                    #ls flag is true, so create a list with the files of the server's directory.
                    if(rcvmsg.get_ls()==1):
                        arr = os.listdir(rcvmsg.get_filename())
                        
                        for i in range(0,len(arr)):
                            path=arr[i]
                            if(os.path.isdir(path)==1):
                                arr[i] = arr[i]+"/"
                        
                        reply=nfs_reply_message(0,0,0,rcvmsg.get_sequence(),arr,0)
                        reply=pickle.dumps(reply)
                        sock.sendto(reply,addr)
                    
                    else:
                        tmod_openRPC_check=0
                        
                        #Check if there is an nfs_server_node with the same filename and the same flags.
                        for i in range(0,len(nfs_server_list)):
                            if(nfs_server_list[i].get_filename()==rcvmsg.get_filename() and nfs_server_list[i].get_flags()==rcvmsg.get_flags()):
                                #Update active time, because we just used this nfs_server_list node.
                                nfs_server_list[i].set_active_time(time.time()-start_time)
                                reply=nfs_reply_message(Path(rcvmsg.get_filename()).stat().st_size,nfs_server_list[i].get_fd_client(),nfs_server_list[i].get_tmod(),rcvmsg.get_sequence(),None,0)
                                reply=pickle.dumps(reply)
                                sock.sendto(reply,addr)
                                tmod_openRPC_check=1
                                break
                            
                        if(tmod_openRPC_check==1):
                            #There is a nfs_server_node with the same filename and the same flags, so this is an openRPC message for tmod check.
                            print("Received an openRPC message for tmod check")
                            lock.release()
                            continue
                        
                        fd=-1
                        #Try to open the file.
                        try:
                            fd=os.open(rcvmsg.get_filename(),rcvmsg.get_flags())
                        except IOError:
                            print("Error in open")
                            reply=nfs_reply_message(0,-1,0,rcvmsg.get_sequence(),None,0)
                            reply=pickle.dumps(reply)
                            sock.sendto(reply,addr)
                            lock.release()
                            continue
                        
                        new_tmod=0
                        #If the "flags" variable contains the os.O_TRUNC mode, update the tmod variable.
                        if(rcvmsg.get_flags()==os.O_RDONLY|os.O_TRUNC or rcvmsg.get_flags()==os.O_WRONLY|os.O_TRUNC or rcvmsg.get_flags()==os.O_RDWR|os.O_CREAT|os.O_TRUNC):
                            new_tmod=update_tmod(rcvmsg.get_filename())
                        
                        increase_num += 1
                        #Create a new file descriptor and add it to the reply message.
                        fd_client = increase_num
                        
                        print("Return to client fd_client :",fd_client)
                        reply=nfs_reply_message(Path(rcvmsg.get_filename()).stat().st_size,fd_client,new_tmod,rcvmsg.get_sequence(),None,0)
                        reply=pickle.dumps(reply)
                        sock.sendto(reply,addr)
                        
                        #Create a new nfs_node and append it into the nfs_list.
                        newnode = nfs_server_node(fd_client,fd,0,0,rcvmsg.get_filename(),rcvmsg.get_flags(),time.time()-start_time)
                        nfs_server_list.append(newnode)
                    
                elif(rcvmsg.get_message_type()=="READ"):
                    #Received a readRPC message.
                    
                    tmod,fd_OS,num_of_bytes=0,0,0
                    
                    for i in range(0,len(nfs_server_list)):
                        if(nfs_server_list[i].get_fd_client()==rcvmsg.get_fd_client()):
                            #Update active time, because we just used this nfs_server_list node.
                            nfs_server_list[i].set_active_time(time.time()-start_time)
                            tmod=nfs_server_list[i].get_tmod()
                            fd_OS=nfs_server_list[i].get_fd_OS()
                            break
                        
                    #This is EOF. Set the num_of_bytes variable to 0.
                    if(rcvmsg.get_pos()==Path(rcvmsg.get_filename()).stat().st_size):
                        reply=nfs_reply_message(0,0,tmod,rcvmsg.get_sequence(),data,0)
                        reply=pickle.dumps(reply)
                        sock.sendto(reply,addr)
                        lock.release()
                        continue
                    
                    try:
                        #Seek to the position specified by the rcvmsg.get_pos() and read a block of data from the file.
                        os.lseek(fd_OS,int(rcvmsg.get_pos()/BLOCK_SIZE)*BLOCK_SIZE,0)
                        data=os.read(fd_OS,BLOCK_SIZE)
                        num_of_bytes = len(data)
                    except IOError:
                        print("Error during reading")
                        reply=nfs_reply_message(0,-1,0,rcvmsg.get_sequence(),None,-1)
                        reply=pickle.dumps(reply)
                        sock.sendto(reply,addr)
                        lock.release()
                        continue
                    
                    reply=nfs_reply_message(0,0,tmod,rcvmsg.get_sequence(),data,num_of_bytes)
                    reply=pickle.dumps(reply)
                    sock.sendto(reply,addr)
                    
                elif(rcvmsg.get_message_type()=="WRITE"):
                    #Received an writeRPC message.
                    
                    tmod,fd_OS,num_of_bytes=0,0,0
                    
                    for i in range(0,len(nfs_server_list)):
                        if(nfs_server_list[i].get_fd_client()==rcvmsg.get_fd_client()):
                            #Update active time, because we just used this nfs_server_list node.
                            nfs_server_list[i].set_active_time(time.time()-start_time)
                            tmod=nfs_server_list[i].get_tmod()
                            fd_OS=nfs_server_list[i].get_fd_OS()
                            break
                        
                    try:
                        #Seek to the position specified by the rcvmsg.get_pos() and write the data to the file.
                        os.lseek(fd_OS,rcvmsg.get_pos(),0)
                        num_of_bytes=os.write(fd_OS,rcvmsg.get_data())
                    except IOError:
                        print("Error during writing")
                        reply=nfs_reply_message(0,-1,0,rcvmsg.get_sequence(),None,-1)
                        reply=pickle.dumps(reply)
                        sock.sendto(reply,addr)
                        lock.release()
                        continue
                    
                    #Increase the tmod variable due to writing.
                    tmod=update_tmod(rcvmsg.get_filename())
                    
                    reply=nfs_reply_message(Path(rcvmsg.get_filename()).stat().st_size,0,tmod,rcvmsg.get_sequence(),0,num_of_bytes)
                    reply=pickle.dumps(reply)
                    sock.sendto(reply,addr)
                    
            else:
                if(rcvmsg.get_message_type()=="OPEN"):
                    #This is a duplicate openRPC message.
                    print("This is a duplicate openRPC message")
                    
                    #ls flag is true, so create a list with the files of the server's directory.
                    if(rcvmsg.get_ls()==1):
                        arr = os.listdir(rcvmsg.get_filename())
                        
                        for i in range(0,len(arr)):
                            path=arr[i]
                            if(os.path.isdir(path)==1):
                                arr[i] = arr[i]+"/"
                        
                        reply=nfs_reply_message(0,0,0,rcvmsg.get_sequence(),arr,0)
                        reply=pickle.dumps(reply)
                        sock.sendto(reply,addr)
                    
                    else:
                        for i in range(0,len(nfs_server_list)):
                            #There is an nfs_server_node with the same filename and the same flags inside the nfs_server_list.
                            if(rcvmsg.get_filename()==nfs_server_list[i].get_filename() and rcvmsg.get_flags()==nfs_server_list[i].get_flags()):
                                #Update active time, because we just used this nfs_server_list node.
                                nfs_server_list[i].set_active_time(time.time()-start_time)
                                reply=nfs_reply_message(Path(rcvmsg.get_filename()).stat().st_size,nfs_server_list[i].get_fd_client(),0,rcvmsg.get_sequence(),None,0)
                                reply=pickle.dumps(reply)
                                sock.sendto(reply,addr)
                                #Do not create a new nfs_node.
                                break
    
                elif(rcvmsg.get_message_type()=="READ"):
                    #This is a duplicate readRPCmessage.
                    print("This is a duplicate readRPCmessage")
                    tmod,fd_OS,num_of_bytes=0,0,0
                    
                    for i in range(0,len(nfs_server_list)):
                        if(nfs_server_list[i].get_fd_client()==rcvmsg.get_fd_client()):
                            #Update active time, because we just used this nfs_server_list node.
                            nfs_server_list[i].set_active_time(time.time()-start_time)
                            tmod=nfs_server_list[i].get_tmod()
                            fd_OS=nfs_server_list[i].get_fd_OS()
                            #print("pos is ", int(rcvmsg.get_pos()/BLOCK_SIZE)*BLOCK_SIZE)
                            break
                        
                    #This is EOF. Set the num_of_bytes variable to 0.
                    if(rcvmsg.get_pos()==Path(rcvmsg.get_filename()).stat().st_size):
                        reply=nfs_reply_message(0,0,tmod,rcvmsg.get_sequence(),data,0)
                        reply=pickle.dumps(reply)
                        sock.sendto(reply,addr)
                        lock.release()
                        continue
                    
                    try:
                        #Seek to the position specified by the rcvmsg.get_pos() and read a block of data from the file.
                        os.lseek(fd_OS,int(rcvmsg.get_pos()/BLOCK_SIZE)*BLOCK_SIZE,0)
                        data=os.read(fd_OS,BLOCK_SIZE)
                        num_of_bytes = len(data)
                    except IOError:
                        print("Error during reading")
                        reply=nfs_reply_message(0,-1,0,rcvmsg.get_sequence(),None,-1)
                        reply=pickle.dumps(reply)
                        sock.sendto(reply,addr)
                        lock.release()
                        continue
                    
                    reply=nfs_reply_message(0,0,tmod,rcvmsg.get_sequence(),data,num_of_bytes)
                    reply=pickle.dumps(reply)
                    sock.sendto(reply,addr)
                elif(rcvmsg.get_message_type()=="WRITE"):
                    #This is a duplicate writeRPCmessage.
                    print("This is a duplicate writeRPCmessage")
                    
                    tmod,fd_OS,num_of_bytes=0,0,0
                    
                    for i in range(0,len(nfs_server_list)):
                        if(nfs_server_list[i].get_fd_client()==rcvmsg.get_fd_client()):
                            #Update active time, because we just used this nfs_server_list node.
                            nfs_server_list[i].set_active_time(time.time()-start_time)
                            tmod=nfs_server_list[i].get_tmod()
                            fd_OS=nfs_server_list[i].get_fd_OS()
                            break
                        
                    #Do not update tmod, do not write the data again.
                    num_of_bytes=len(rcvmsg.get_data())
                    reply=nfs_reply_message(Path(rcvmsg.get_filename()).stat().st_size,0,tmod,rcvmsg.get_sequence(),0,num_of_bytes)
                    reply=pickle.dumps(reply)
                    sock.sendto(reply,addr)
            lock.release()
    
    
#This is the garbage collector.
def garbage_collector():
    global lock
    global GARBAGE_COLLECTOR_TIME_INTERVAL
    
    while(1):
        #Sleep for GARBAGE_COLLECTOR_TIME_INTERVAL seconds.
        time.sleep(GARBAGE_COLLECTOR_TIME_INTERVAL)
        lock.acquire()
        print("------GARBAGE COLLECTOR ON------")
        for j in range(0,len(nfs_server_list)):
            for i in range(0,len(nfs_server_list)):
                #If this node is unused for more than GARBAGE_COLLECTOR_TIME_INTERVAL seconds, delete this node.
                if(time.time()-start_time - nfs_server_list[i].get_active_time() > GARBAGE_COLLECTOR_TIME_INTERVAL):
                    print("Delete fd:",nfs_server_list[i].get_fd_client(),",filename:",nfs_server_list[i].get_filename())
                    del nfs_server_list[i]
                    break
        print("------GARBAGE COLLECTOR OFF------")
        lock.release()
    
if __name__ == "__main__":
    main()    
    
