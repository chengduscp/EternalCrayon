/*Michael Chan CS118 Project 1*/
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define BUFF_SIZE 1024
void error(char *msg)
{
   perror(msg);
   exit(1);
}

void writeResponse(char *response, char *filename)
{
   struct stat st;
   long size;
   time_t lastModTime;
   long tempSize;
   long val;
   int curDig;
   long length; 
   char* sizeBuffer;
   char* stringStack;
   char* lastModTimeStr;
   char c[2];
   int i, j;
   int sizeOfSizeBuffer;
   char *fileContents;
   FILE *f = fopen(filename, "rb");

   

   if(f)
   {


      stat(filename, &st);
      size = (long) st.st_size;
      lastModTime = st.st_mtime;
      lastModTimeStr = (char *)malloc(26*sizeof(char));
      lastModTimeStr = asctime(localtime(&lastModTime));
      printf("%s\n", lastModTimeStr);
      val = 10;
      sizeOfSizeBuffer = 1;
      /*determine how many digits are in file size*/
      while(size > val)
      {
         val = val * 10;
         sizeOfSizeBuffer++;
      }
      sizeBuffer  = (char *)malloc(sizeOfSizeBuffer+1);
      stringStack = (char *)malloc(sizeOfSizeBuffer+1);

      /*convert size to a string*/
      tempSize = size;
      curDig = tempSize % 10;
      curDig = curDig + ((int)'0');
      c[0] = (char) curDig;
      c[1] = '\0';
       
      strcpy(stringStack, c);
      tempSize = tempSize / 10;
      while(tempSize > 0)
      {
         curDig = tempSize % 10;
         curDig = curDig + ((int)'0');

         c[0] = (char) curDig;
         c[1] = '\0';
         strcat(stringStack, c);
         tempSize = tempSize / 10;
                  
      }
      j = sizeOfSizeBuffer - 1;
      for(i = 0; i < sizeOfSizeBuffer ; i++)
      {
         sizeBuffer[i] = stringStack[j];
         j--;
      }
      sizeBuffer[sizeOfSizeBuffer] = '\0';
      fileContents = malloc(size);
      if(fileContents)
      {
        fread(fileContents, 1, size, f);
      }
      fclose(f);

      strcpy(response, "HTTP/1.1 200 OK\r\nConnection: close\r\n");
   }
   else
   {
      strcpy(response, "HTTP/1.1 404 Not Found\r\nConnection: cloe\r\n");
   }
   //itoa(size, sizeBuffer, 10);
   //strcpy(sizeBuffer, "166");




   strcat(response, "Date: Mon, 14 Oct 2013 22:09:53 PDT\r\n");
   strcat(response, "Server: Apache/2.2.3 (CentOS)\r\n");
   strcat(response, "Last-Modified: ");
   strcat(response, lastModTimeStr);
//   strcat(response, "\r\n");
   strcat(response, "Content-Length: ");
   strcat(response, sizeBuffer);
   strcat(response, "\r\n");
   strcat(response, "Content-Type: text/html\r\n\r\n");
   if(fileContents)
   {
      if(f)
      {
         strcat(response, fileContents);
         strcat(response, "\r\n");
      }
   } 
    
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
               char httpResponse[10000];     

 
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
                  printf("Here is the message:\n%s\n**************************************************\n", buffer);
                  /*writeBytes = write(i, "I got your message!\n", 20);
                  if(writeBytes < 0)
                     error("Error writing to socket");*/

                  // Clean up TCP connection
                  writeResponse(httpResponse, "sample.html");
                  printf("Response:\n%s\n", httpResponse);
                  writeBytes = write(i, httpResponse, strlen(httpResponse));
                  if(writeBytes < 0)
                     error("Error writing to socket");
                  close(i); // When we write more complicated things, we will have to figure this out better
                  FD_CLR(i, &active_fd_set);
               }
            }
         }
      
      }
   }

 
   close(sockfd);

   return 0;

}
