#Distributed systems application project
#Author : Angelis Marios
#NFS (Network File System) implementation using python
#We use exactly once semantics and a stop-and-wait networking protocol
from nfs_client_middleware import *
import time
import sys
import os
import random
import filecmp

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
            
        fd2=mynfs_open("server_files/local_to_remote_photo.jpg",os.O_RDWR|os.O_CREAT|os.O_TRUNC)
        
        if(fd2==-1):
            print("Error in my_nfs_open")
            sys.exit()    
            
        try:
            fd3=os.open("photo_local.jpg",os.O_RDWR|os.O_CREAT|os.O_TRUNC)
        except IOError:
            print("Error in os open")
            sys.exit()
            
        time.sleep(2)
        end1=0
        #read data from "server_files/argv[3]" file and write it in the "photo_local.jpg" file and in the "server_files/local_to_remote_photo.jpg" file.
        while(1):
            if(end1==0):
                data,nofbytes=mynfs_read(fd1,50);
                if(nofbytes==-1):
                    print("Error in mynfs_read")
                    sys.exit()
                if(nofbytes==50):
                    os.write(fd3,data)
                    mynfs_write(fd2,data,len(data))
                else:
                    os.write(fd3,data)
                    mynfs_write(fd2,data,len(data))
                    end1=1

            if(end1==1):
                break
             
        try:
            fd4=os.open("local_to_remote_photo_final_local.jpg",os.O_RDWR|os.O_CREAT|os.O_TRUNC)
        except IOError:
            print("Error in os open")
            sys.exit()
            
        time.sleep(2)
        mynfs_lseek(fd2,0,os.SEEK_SET)
        end1=0
        
        #read data from "server_files/local_to_remote_photo.jpg" file and write it in the "local_to_remote_photo_final_local.jpg" file.
        while(1):
            if(end1==0):
                data,nofbytes=mynfs_read(fd2,50);
                if(nofbytes==-1):
                    print("Error in mynfs_read")
                    sys.exit()
                if(nofbytes==50):
                    os.write(fd4,data)
                else:
                    os.write(fd4,data)
                    end1=1

            if(end1==1):
                break
        
        mynfs_close(fd1)
        mynfs_close(fd2)    
                    
        #compare "local_to_remote_photo_final_local.jpg" and "photo_local.jpg" files.
        comp = filecmp.cmp("local_to_remote_photo_final_local.jpg","photo_local.jpg")
        
        if(comp==1):
            print("## No difference between two files ##")
        else:
            print("## Binary files are not same ##")
        
