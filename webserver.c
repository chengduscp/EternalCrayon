/*Michael Chan CS118 Project 1*/
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>
#include <signal.h>

void error(char *msg)
{
   perror(msg);
   exit(1);
}

int main(int argc, char *argv[])
{
   int sockfd, newsockfd, portno, pid;
   struct sockaddr_in server, client;
   
   if(argc < 2)
   {
      fprintf(stderr, "ERROR, no port provided\n");
   }
   sockfd = socket(AF_INET, SOCK_STREAM,0);
   if(sockfd < 0)
      error("SOCKET");
   
   memset((char *) &server, 0, sizeof(server));
   portno = atoi(argv[1]);
   server.sin_family = AF_INET;
   server.sin_addr.s_addr = INADDR_ANY;
   server.sin_port=htons(portno);


   close(sockfd);

}
