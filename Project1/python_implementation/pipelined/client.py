from middleware import *
import time
import sys
import random

if __name__ == "__main__":
    #argv[1] is the service id and argv[2] is client's socket reply port
    if(len(sys.argv)<3):
        print("Wrong number of arguments")
        sys.exit();
    else:
        #Read 1000 numbers from a file
        fo = open("primes.txt", "r+")
        for j in range(0,1000):
            for i in range(0,5):
                #client reads a number from input and calls sendRequest
                number = fo.readline()
                if not number:
                    print("EOF")
                    fo.close()
                    __exit__
                while(1):
                    ret=sendRequest(int(number),int(sys.argv[1]),int(sys.argv[2]))
                    if(ret==-1):
                        print("No servers are available,do you want to try again?(Y/n)",end=" ")
                        try_again=input()
                        print(try_again)
                        if(try_again=='n'):break
                    else:break
            #After giving 5 numbers,client tries to get reply of these requests
            time.sleep(1)
            for i in range(0,100):
                reply_message=getReply()
                if(reply_message==0):
                    goo=1
                else:print(reply_message)
