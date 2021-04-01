#Distributed systems application project
#Author : Angelis Marios
#NFS (Network File System) implementation using python
#We use exactly once semantics and a stop-and-wait networking protocol
from nfs_client_middleware import *
import time
import sys
import os
TGREEN =  '\033[32m' 
ENDC = '\033[m'
BLUE='\033[94m'

if __name__ == "__main__":
    #argv[1] is the server's ip and argv[2] is the server's port and argv[3] is file name in the server side
    #python3 nfs_test.py 192.168.2.49 5000 
    if(len(sys.argv)<3):
        print("Wrong number of arguments")
    else:
        path="./"
        data=None
        mynfs_set_srv_addr(sys.argv[1],int(sys.argv[2]))
        mynfs_set_cache(8192,5)
        
        while(1):
            choice=input()
            token = choice.split(" ")
            if(token[0]=="ls"):
                #ls command
                data=mynfs_ls(path)
                for i in range(0,len(data)):
                    split_path=data[i].split("/")
                    if(len(split_path)==1):
                        print(TGREEN +split_path[0]+ENDC)
                    else:
                        print(TGREEN +split_path[len(split_path)-2]+"/"+ENDC)
 
            elif(token[0]=="cd" and len(token)==2 and token[1]!=".."):
                #cd server_files/
                #all directories have a "/" as their last character
                if(token[1][len(token[1])-1]!='/'):
                    print("This is not a directory")
                    continue
                
                path = path + token[1]
                print(BLUE+path+ENDC)
            elif(token[0]=="cd" and len(token)==2 and token[1]==".."):
                #cd ..
                if(path=="./"):
                    print("This is the home directory")
                    continue
                split_path=path.split("/")
                path=""
                for i in range(0,len(split_path)-2):
                    path = path + split_path[i] + "/"
                print(BLUE+path+ENDC)
            elif(token[0]=="pwd"):
                #pwd
                print(BLUE+path+ENDC)
            elif(token[0]=="download"):
                #download lake.jpg local_lake.jpg
                print("download file is",path+token[1],"destination is",token[2])
                
                fd1=mynfs_open(path+token[1],os.O_RDWR)
                if(fd1==-1):
                    print("Error in my_nfs_open")
                    sys.exit() 
                    
                try:
                    fd2=os.open(token[2],os.O_RDWR|os.O_CREAT|os.O_TRUNC)
                except IOError:
                    print("Error in os open")
                    sys.exit()
                
                end1=0
                while(1):
                    if(end1==0):
                        data,nofbytes=mynfs_read(fd1,50);
                        if(nofbytes==-1):
                            print("Error in mynfs_read")
                            sys.exit()
                        if(nofbytes==50):
                            os.write(fd2,data)
                        else:
                            os.write(fd2,data)
                            end1=1

                    if(end1==1):
                        break
        
                mynfs_close(fd1)
            elif(token[0]=="upload"):
                #upload local_lake.jpg server_lake.jpg
                print("upload file is",token[1],"destination is",path+token[2])
                
                fd1=mynfs_open(path+token[2],os.O_RDWR|os.O_CREAT|os.O_TRUNC)
                if(fd1==-1):
                    print("Error in my_nfs_open")
                    sys.exit() 
                
                try:
                    fd2=os.open(token[1],os.O_RDWR)
                except IOError:
                    print("Error in os open")
                    sys.exit()
                
                while(1):
                    try:
                        data=os.read(fd2,4000)
                    except IOError:
                        print("Error during reading")
                        sys.exit()
                    
                    if(len(data)==0):
                        print("EOF")
                        break
                    
                    ret=mynfs_write(fd1,data,len(data))
                    if(ret==-1):
                        print("Error during writing")
                        sys.exit()
            
                mynfs_close(fd1)
                
            elif(token[0]=="exit"):
                #exit
                sys.exit()
            else:
                print("command not found")
