Angelis Marios-Kasidakis Theodoros
AEM:2406-2258

Manager compilation : gcc -Wall -g manager.c -o manager -lpthread
Member compilation : gcc -Wall -g member.c -o member -lpthread

Manager execution : ./manager <manager_ip> <manager port>
Member execution : ./member <group_name> <manager_ip> <manager port> <ack_port> <member_ip>
