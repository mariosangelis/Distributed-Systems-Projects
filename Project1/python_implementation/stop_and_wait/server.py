import time
from middleware import *
import sys
def primetest(v):
    flag=0
    for k in range(2,int(v/2)):
        if(v%k==0):
            flag=1
            break
    if(v==1):return 2
    else:
        if(flag==0):return 0
        else:return 1
    return 0

if __name__ == "__main__":
    #argv[1] is the service id and argv[2] is server's socket port
    if(len(sys.argv)<3):
        print("Wrong number of arguments")
    else:
        register (int(sys.argv[1]),int(sys.argv[2]));
    while(1):
        time.sleep(1)
        #Get a request
        request=getRequest()
        if(request!=0):
            number=request[0]
            ret=primetest(number)
            #Serve this request
            if(ret==0): message=str(number) + " is a prime number"
            elif(ret==1):message=str(number) + " is not a prime number"
            else:message=str(number) + " is neither a prime number nor a composite number"
            #Send the reply back to the client
            sendReply(message,request[1])
        else:
            print("No request found")
