/*Michael Chan CS118 Project 1*/
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <errno.h>

#define BUFF_SIZE 1024
void error(char *msg)
{
   perror(msg);
   exit(1);
}

int main(int argc, char *argv[])
{
   int sockfd, newsockfd, portno, pid, rc, maxfd;
   struct sockaddr_in server, client;
   socklen_t clientLen;   
   int end_server = 0;
   int i;
   fd_set active_fd_set, read_fd_set;
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


   if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
      error("BIND ERROR");

   rc = listen(sockfd,2);

   if(rc < 0)
   {
      error("listen failed");
      close(sockfd);
      exit(-1);
   }

   FD_ZERO(&active_fd_set);
   FD_SET(sockfd, &active_fd_set);
   while(1)
   {

      read_fd_set = active_fd_set;
      if(select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
      {
         error("SELECT ERROR");
      }
      for(i = 0; i < FD_SETSIZE ; i++)
      {

         if(FD_ISSET(i, &read_fd_set))
         {
            if(i==sockfd)
            {
               int new;
               new = accept(sockfd,
                           (struct sockaddr *) &client,
                           &clientLen);
               if(new < 0)
               {
                  error("ERROR ON ACCEPT");
               }
               FD_SET(new, &active_fd_set);
            }

            else
            {

               int readBytes, writeBytes;
               char buffer[BUFF_SIZE];
      
               memset(buffer, 0, BUFF_SIZE*sizeof(char));
               readBytes = read(i, buffer, BUFF_SIZE-1);
               if(readBytes < 0)
               {
                  error("Error reading from socket");
               }
               else if(readBytes == 0)
               {
                  close(i);
                  FD_CLR(i, &active_fd_set);
               }
               else
               {
                  printf("Here is the message: %s\n**************************************************\n", buffer);
                  
 
                  writeBytes = write(i, "I got your message", 18);

                  if(writeBytes < 0)
                     error("Error writing to socket");
               }
            }
         }
      
      }
   }

 
   close(sockfd);

   return 0;

}
