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
typedef enum
{
   TEXT_HTML,
   IMAGE_JPEG,
   IMAGE_GIF,
   OTHER
} content_type_t;
void error(char *msg)
{
   perror(msg);
   exit(1);
}

/* this file reads the filecontents and stores it in fileBuf*/
static int inline getContents(char* filename, int* fSize, char* fileBuf)
{
   
   FILE *f = fopen(filename, "rb");
   if(f)
   {
      if(fileBuf)
      {
        *fSize = fread(fileBuf, 1, *fSize, f);
      }
      fclose(f);
      return 1;
   }
   else
      return 0;
}

/*this file takes care of creating the HTTP response except for the file contents*/
static void inline writeResponse(char *response, char *filename, content_type_t content_type, int* fSize )
{
   struct stat st;
   long size, tempSize, val, strSize;
   time_t lastModTime, curTime;
   int curDig , i , j, sizeOfSizeBuffer;
   char *sizeBuffer, *stringStack, *lastModTimeStr, *timeOfResponse;
   char c[2];
   FILE *f = fopen(filename, "rb");
    

   if(f)
   {
      /* get last modified time and file size*/
      stat(filename, &st);
      size = (long) st.st_size; 
      lastModTime = st.st_mtime;

      /* convert last modified time string */
      lastModTimeStr = (char *)malloc(26*sizeof(char));
      lastModTimeStr = asctime(localtime(&lastModTime));

      /*determine how many digits are in file size*/
      val = 10;
      sizeOfSizeBuffer = 1;
      while(size > val)
      {
         val = val * 10;
         sizeOfSizeBuffer++;
      }
      sizeBuffer  = (char *)malloc(sizeOfSizeBuffer+1);
      stringStack = (char *)malloc(sizeOfSizeBuffer+1);

      /*convert size to a string*/
      tempSize = size;
      *fSize   = size;
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
      fclose(f);

      strcpy(response, "HTTP/1.1 200 OK\r\nConnection: close\r\n");
   }
   else
   {
      strcpy(response, "HTTP/1.1 404 Not Found\r\nConnection: close\r\n");
   }

   /*populate response fields */
   curTime = time(NULL);
   timeOfResponse = (char *)malloc(26*sizeof(char));
   timeOfResponse = asctime(localtime(&curTime));
   strcat(response, "Date: ");
   strcat(response, timeOfResponse);
   strcat(response, "Server: Apache/2.2.3 (CentOS)\r\n");

   if(f)
   {
      strcat(response, "Last-Modified: ");
      strcat(response, lastModTimeStr);
      strcat(response, "Content-Length: ");
      strcat(response, sizeBuffer);
      strcat(response, "\r\n");

      if(content_type == TEXT_HTML)
         strcat(response, "Content-Type: text/html\r\n\r\n");
      else if(content_type == IMAGE_JPEG)
         strcat(response, "Content-Type: image/jpeg\r\n\r\n");
      else if(content_type == IMAGE_GIF)
         strcat(response, "Content-Type: image/gif\r\n\r\n");
      else
         strcat(response, "Content-Type: \r\n\r\n");
   }
   else
   {
      strcat(response, "Last-Modified: \r\n");
      strcat(response, "Content-Length: 30\r\n");
      strcat(response, "Content-Type: \r\n\r\n");
      strcat(response, "HTTP/1.1 404 Content Not Found");
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

               int t;
               int readBytes, writeBytes;
               char buffer[BUFF_SIZE];
               char httpResponse[1000000];     
               char* fileBuf;
               int fSize;
 
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
                  
                  writeResponse(httpResponse, "face.gif", IMAGE_GIF,
                                &fSize);
                  fileBuf = (char *)malloc(fSize*sizeof(char));
                  writeBytes = send(i, httpResponse, strlen(httpResponse),0);
                  if(writeBytes < 0)
                     error("Error writing to socket");

                  if( getContents("face.gif", &fSize, fileBuf) == 1)
                     writeBytes = send(i, fileBuf, fSize, 0);
                 
                  if(writeBytes < 0)
                     error("Error writing to socket");

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
