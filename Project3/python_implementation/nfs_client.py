#Distributed systems application project
#Author : Angelis Marios
#NFS (Network File System) implementation using python
#We use exactly once semantics and a stop-and-wait networking protocol
from nfs_client_middleware import *
import time
import sys
import os
import random

if __name__ == "__main__":
    #argv[1] is the server's ip and argv[2] is the server's port and argv[3] is file name in the server side
    #python3 nfs_client.py 192.168.2.49 5000 server_files/photo.jpg
    if(len(sys.argv)<4):
        print("Wrong number of arguments")
    else:
        mynfs_set_srv_addr(sys.argv[1],int(sys.argv[2]))
        mynfs_set_cache(8192,5)

        time.sleep(3);
        
        fd1=mynfs_open(sys.argv[3],os.O_RDWR|os.O_CREAT)
        
        if(fd1==-1):
            print("Error in my_nfs_open")
            sys.exit() 
            
        fd2=mynfs_open(sys.argv[3],os.O_RDWR|os.O_CREAT)
        
        if(fd2==-1):
            print("Error in my_nfs_open")
            sys.exit()    
            
        try:
            fd3=os.open("lake1.jpg",os.O_RDWR|os.O_CREAT|os.O_TRUNC)
            fd4=os.open("lake2.jpg",os.O_RDWR|os.O_CREAT|os.O_TRUNC)
        except IOError:
            print("Error in os open")
            sys.exit()
            
        time.sleep(2)
        end1,end2=0,0
        first=0
        
        #read data from "server_files/argv[3]" file and write it alternately in lake1.jpg and lake2.jpg files.
        while(1):  
            if(end1==0):
                data,nofbytes=mynfs_read(fd1,50);
                if(nofbytes==-1):
                    print("Error in mynfs_read")
                    sys.exit()
                if(nofbytes==50):
                    os.write(fd3,data)
                else:
                    os.write(fd3,data)
                    end1=1
            if(end2==0):
                data,nofbytes=mynfs_read(fd2,50);
                if(nofbytes==-1):
                    print("Error in mynfs_read")
                    sys.exit()
                if(nofbytes==50):
                    os.write(fd4,data)
                else:
                    os.write(fd4,data)
                    end2=1
            if(first==0):
                first=1
                time.sleep(50)
            if(end1==1 and end2==1):
                break
             
        
        time.sleep(2)       
        fd5=mynfs_open("server_files/s.png",os.O_RDWR|os.O_CREAT|os.O_TRUNC)
        
        if(fd5==-1):
            print("Error in my_nfs_open")
            sys.exit() 
        
        try:
            fd6=os.open("client_s.png",os.O_RDWR|os.O_CREAT)
        except IOError:
            print("Error in os open")
            sys.exit()
            
        time.sleep(40)    
        
        #read data from "client_s.png" file and write it in the "server_files/s.png" file.
        while(1):
            try:
                data=os.read(fd6,4000)
            except IOError:
                print("Error during reading")
                sys.exit()
            
            if(len(data)==0):
                print("EOF")
                break
            
            ret=mynfs_write(fd5,data,len(data))
            if(ret==-1):
                print("Error during writing")
                sys.exit()
        
        mynfs_close(fd1)
        mynfs_close(fd2)    
        mynfs_close(fd5)
