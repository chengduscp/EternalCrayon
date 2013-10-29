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
#define FILE_INTERVAL 1024
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

//////////////////////////////////////////////////////////////////
// Parser
//////////////////////////////////////////////////////////////////
// Error handling
//############################################################
const char END = -1;

void parser_error(const char *err) {
   fprintf(stderr, "PARSER ERROR: %s\n", err);
   // exit(1);
}

// Types
//############################################################
const int BEGINNING_OF_FILE = -12; // Before we have parsed anything

// Token types we can have (semantic label)
typedef enum token_type_ {
   NONE, // Initial state
   TOKEN, // Nothing special
   SEPERATOR, // One of the separator characters defined in RFC 1945
   LINEEND, // End of a line (needs to be carriage return then linefeed)
   TERMINATE // Last token
} token_type_t;

// Dynamcially growing string
typedef struct dy_str_ {
   unsigned len; // Length of string (not including null character)
   unsigned cap; // Capacity of str's buffer
   char *str; // Actual characters (null terminated)
} dy_str_t;

// Fundamental unit of the parser
typedef struct token_ {
   token_type_t type;
   char c; // For identifying special types of tokens
   dy_str_t string;
} token_t;

// Parser to read the HTTP Header
typedef struct parser_ {
   int (*get)(void*); // Get next character
   void *args; // Args for get
   int last; // Last drawn character
} parser_t;

typedef enum file_type_ {
   UNKNOWN_FILE_TYPE,
   PLAIN_TEXT,
   HTML,
   JPEG,
   GIF
} file_type_t;

typedef enum accept_type_ {
   NO_ACCEPT_TYPE,
   ANYTHING,
   HTML_FILE,
   IMAGE_FILE
} accept_type_t;

typedef struct http_header_ {
   char *file_name;
   file_type_t file_type;
   unsigned int host_ip, port;
   // We can ignore User-Agent
   accept_type_t accept_type;
   // We can ignore the other fields
   int keep_connection; // Boolean field
} http_header_t;

typedef struct string_arg_ {
   int str_pos;
   const char* str;
} string_arg_t;

// Dynamic string functions
//############################################################
// We assume you allocate the memory
void create_dy_str(dy_str_t* s)
{
   static const int INITIAL_SIZE = 8;
   s->str = (char *)malloc(INITIAL_SIZE);
   s->str[0] = '\0';
   s->cap = INITIAL_SIZE;
   s->len = 0;
}

// Add character to the token string
inline void add_c(dy_str_t *s, char c)
{
   if (s->len + 1 >= s->cap)
   {
      s->cap *= 2;
      s->str = realloc(s->str, s->cap);
      if(!s->str) {
         parser_error("Couldn't create memory for token");
         exit(1); // Memory error, don't know what to do
      };
   }
   s->str[s->len] = c;
   s->str[s->len + 1] = '\0';
   s->len++;
}

// Assumes c is null-terminated
void add_str(dy_str_t *s, const char *c)
{
   while(*c) add_c(s, *c++);
}


void delete(dy_str_t *s)
{
   if (!s) return;
   if(s->str) free(s->str);
   s->str = 0;
   s->len = 0;
   s->cap = 0;
}


// Basic token functions
//############################################################
token_t* create_token()
{
   token_t *t = (token_t *)malloc(sizeof(token_t));
   t->type = NONE;
   t->c = '\0';
   create_dy_str(&t->string);
   return t;
}

// Printout for debugging
void print_t(token_t *t)
{
   if(!t)
   {
      printf("NULL\n");
      return;
   }
   switch(t->type)
   {
      case NONE:
         printf("Not initialized\n");
         break;
      case TOKEN:
         printf("Regular token: %s\n", t->string.str);
         break;
      case SEPERATOR:
         printf("Seperator token: %c\n", t->c);
         break;
      case LINEEND:
         printf("Line end token: \\r\\n\n");
         break;
      case TERMINATE:
         printf("End of file token\n");
         break;
      default:
         printf("Unknown type\n");
         break;
   }
}
// Add character to the token string
void add(token_t *t, char c)
{ add_c(&t->string, c); }

// Compare a string/character with the token's
int token_cmp(token_t *t, const char *c)
{
   if (!t || !t->string.str || t->type != TOKEN)
      return 1;
   else
      return strcmp(t->string.str, c);
}

int token_cmp_c(token_t *t, token_type_t type, const char c)
{
   if (!t || t->type != type)
      return 1;
   else
      return (t->c - c);
}

// Error checking
inline int error_token(token_t *t)
{ return (!t || !t->string.str || t->type == NONE); }

// Clean up functions
void delete_string(token_t *t)
{
   if (!t) return;
   delete(&t->string);
}

void delete_token(token_t *t)
{
   if(!t) return;
   delete_string(t);
   free(t);
   t = 0;
}


// Basic parser functions
//############################################################

int is_seperator(int c)
{
   switch(c)
   {
      case '(':
      case ')':
      case '<':
      case '>':
      case '@':
      case ',':
      case ';':
      case ':':
      case '\\':
      case '\"':
      case '/':
      case '[':
      case ']':
      case '?':
      case '=':
      case '{':
      case '}':
      case ' ':
      case '\t': // Seperators defined in RFC 1945
         return 1;
         break;
      default:
         if (iscntrl(c))
            return 1;
         break;
   }
   return 0;
}

int is_line_end(int c) { return (c == '\r' || c == '\n'); }

int is_eof(int c) { return (c < 0); }

// Gets a token of the HTTP header
token_t* get_token(parser_t *p)
{
   token_t *t = create_token();
   int next;
   t->type = TOKEN; // Default
   next = p->last;
   // Trim whitespace from tokens
   while(isblank(next) || next == BEGINNING_OF_FILE) {
      next = p->get(p->args);
   }
   // Get the actual token
   while (!(is_seperator(next) || is_line_end(next) || is_eof(next)))
   {
      char c = (char)next;
      add(t,c);
      p->last = next;
      next = p->get(p->args);
   }
      // printf("Got here\n");

   // Check that we actually have characters in our token
   if(t->string.len == 0)
   {
      // See why this has no characters
      if (is_line_end(next)) // End of line
      {
         p->last = next;
         next = p->get(p->args);
         if (is_eof(next)) // File ended with a line termination
         {
            delete_string(t);
            t->type = TERMINATE;
         }
         else
         {
            if (p->last != '\r' || next != '\n') {
               parser_error("Line Ending Does not conform to standard");
               delete_token(t);
               return 0;
            } else {
               next = p->get(p->args); // Don't care about the newline anymore
               delete_string(t);
               t->c = '\n';
               t->type = LINEEND;
            }
         }
      }
      else if (next < 0) // End of File
      {
         delete_string(t);
         t->c = '\0';
         t->type = TERMINATE;
      }
      else if (is_seperator(next)) // Separator token
      {
         delete_string(t);
         char c = (char)next;
         t->c = c;
         next = p->get(p->args); // Need to advance
         t->type = SEPERATOR;
      }
      else { // Don't know what happened
         parser_error("Unknown Token");
         delete_token(t);
         return 0;
      }
   }
   p->last = next;
   return t;
}

// Parses the HTTP Header

// Get the next character from the header string (used in the parser)
int getFromString(void *arg)
{
   string_arg_t *s = (string_arg_t *)arg;
   if (s->str[s->str_pos] == '\0')
   {
      return -1;
   }
   s->str_pos++;
   return (int)(s->str[s->str_pos - 1]);
}

// For debugging
print_file_type(file_type_t *ft) {
   switch(*ft) {
      case UNKNOWN_FILE_TYPE:
         printf("File type: UNKNOWN_FILE_TYPE\n");
         break;
      case PLAIN_TEXT:
         printf("File type: PLAIN_TEXT\n");
         break;
      case HTML:
         printf("File type: HTML\n");
         break;
      case JPEG:
         printf("File type: JPEG\n");
         break;
      case GIF:
         printf("File type: GIF\n");
         break;
      default:
         break;
   }
}

void printParse(const char *c_str)
{
   token_t *t = NULL;
   parser_t p;
   p.get = &getFromString;
   p.args = (void*)c_str;
   p.last = BEGINNING_OF_FILE; // initialize as BEGINNING_OF_FILE
   while ((t = get_token(&p)))
   {
      print_t(t);
      if (t->type == TERMINATE)
         break;
      delete_token(t);
   }
   // Delete the last token
   delete_token(t);
}


#define TOKEN_ASSERTION(t, p, assertion) \
   t = get_token(p); \
   if(!assertion) { \
      delete_token(t); \
      parser_error("Unknown entity in header"); \
      return 0; \
   } \
   delete_token(t); \

#define TOKEN_CHECK(t, p, assertion) \
   t = get_token(p); \
   if(!assertion) { \
      delete_token(t); \
      parser_error("Unknown entity in header"); \
      return; \
   } \
   delete_token(t); \

// Assumes we've just started parsing the header file and just gotten the 'GET'
char * get_filename(parser_t *p, file_type_t *type) {
   token_t *t;
   dy_str_t str;
   int first = 1;
   t = get_token(p);
   create_dy_str(&str);
     // Check for absolute path, store path, etc.
   // Have absolute path
   if (token_cmp_c(t, SEPERATOR, '/') == 0)
   {
      while(token_cmp(t, "HTTP") != 0 && t->type != LINEEND)
      {
           // print_t(t);
         if (!first && token_cmp_c(t, SEPERATOR, '/') == 0)
         {
            add_c(&str, '/');
         }
         else if (t->type == TOKEN)
         {
            add_str(&str, t->string.str);
         }
         first = 0;
         delete_token(t);
         t = get_token(p);
      }
   }
   delete_token(t);

   // Get file type information
   char *c = &str.str[str.len-1];
   // Get the file extention
   if(str.len > 1)
      while(*(c-1) != '.' && *(c-1) != '/') c--;

   // Add appropriate file type
   if (str.len == 0) {
      // Doesn't specify file, assume index.html
      add_str(&str, "index.html");
      *type = HTML;
   } else if (strcmp(c, "html") == 0) {
      *type = HTML;
   } else if (strcmp(c, "jpg") == 0 || strcmp(c, "jpeg") == 0) {
      *type = JPEG;
   } else if (strcmp(c, "gif") == 0) {
      *type = GIF;
   } else if (strcmp(c, "txt") == 0) {
      *type = PLAIN_TEXT;
   } else {
      *type = UNKNOWN_FILE_TYPE;
   }

   TOKEN_ASSERTION(t, p, (token_cmp_c(t, SEPERATOR, '/') == 0));
   TOKEN_ASSERTION(t, p, (t->string.str && atof(t->string.str) > 1.0f));
   TOKEN_ASSERTION(t, p, (t->type == LINEEND));

   return str.str;
}

// Assumes we've just gotten the file name
void get_port(parser_t *p, http_header_t *h) {
   // Initialize to avoid memory bugs
   h->host_ip = 0;
   h->port = 0;

   // Get the header stuff out of the way
   token_t *t = 0;
   TOKEN_CHECK(t, p, (token_cmp(t, "Host") == 0));
   TOKEN_CHECK(t, p, (token_cmp_c(t, SEPERATOR, ':') == 0));

   // Get IP Address
   t = get_token(p);
   if (t->type != TOKEN) {
      fprintf(stderr, "Unable to get IP Address\n");
      return;
   }

   int ip1 = 0, ip2 = 0, ip3 = 0, ip4 = 0;
   if(sscanf(t->string.str, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) < 0) {
      fprintf(stderr, "Unable to get IP Address\n");
      return;
   }
   h->host_ip = ((ip1 & 0xff) << 24) | ((ip2 & 0xff) << 16) | ((ip3 & 0xff) << 8) | (ip4 & 0xff);

   // Get port
   delete_token(t);
   TOKEN_CHECK(t, p, (token_cmp_c(t, SEPERATOR, ':') == 0));
   t = get_token(p);
   if (t->type != TOKEN) {
      fprintf(stderr, "Unable to get port number\n");
      print_t(t);
      return;
   }

   unsigned int port = 0;
   if(sscanf(t->string.str, "%u", &port) < 0) {
      fprintf(stderr, "Unable to get port number\n");
      return;
   }
   delete_token(t);
   h->port = port;
   TOKEN_CHECK(t, p, (t->type == LINEEND));
}

void get_sub_headers(parser_t *p, http_header_t *h) {
   h->accept_type = ANYTHING; // For now, we assume the client accepts whatever file format
   token_t *t = get_token(p);
   while (t->type != LINEEND) { // Double line ending indicates end of header
      if (token_cmp(t, "User-Agent") == 0 ||
            token_cmp(t, "Accept-Language") == 0 ||
            token_cmp(t, "Accept-Encoding") == 0 ||
            token_cmp(t, "Accept") == 0) {
         // Can skip these ones -- maybe implement them later
         // printf("Skipping...\n");
      } else if (token_cmp(t, "Connection") == 0) {
         delete_token(t);
         TOKEN_CHECK(t, p, (token_cmp_c(t, SEPERATOR, ':') == 0));
         t = get_token(p);
         if (token_cmp(t, "keep-alive") == 0) {
            h->keep_connection = 1;
            // printf("Kept connection\n");
         } else {
            h->keep_connection = 0;
            // printf("Did not keep connection\n");
         }
      } else {
         // printf("printing token %s\n", t->string.str ? t->string.str : "something");
         // parser_error("Unknown sub-header type");
      }
      while (t->type != LINEEND) {
         delete_token(t);
         t = get_token(p);
      }
      delete_token(t);
      t = get_token(p);
   }
   delete_token(t);
}

http_header_t *get_header(parser_t *p) {
   http_header_t *h = (http_header_t *)malloc(sizeof(http_header_t));
   h->file_name = get_filename(p, &h->file_type);
   if (h->file_name == 0)
      return h;
   get_port(p, h);
   get_sub_headers(p, h);

   return h;
}

http_header_t *parse_header(const char *header_string)
{
   token_t *t;
   parser_t p;
   p.get = &getFromString;
   string_arg_t arg;
   arg.str = header_string;
   arg.str_pos = 0;
   p.args = (void*)&arg;
   p.last = BEGINNING_OF_FILE; // initialize as BEGINNING_OF_FILE
   // Get the Request type
   t = get_token(&p);
   if (token_cmp(t, "GET") == 0)
   {
      delete_token(t);
      return get_header(&p);
   }
   else if (error_token(t))
   {
      printf("Error parsing header\n");
      print_t(t);
   }
   else
   {
      printf("Unsupported method type\n");
   }
   delete_token(t);
   return 0;
}

void error(char *msg)
{
   perror(msg);
   exit(1);
}
static int inline min(int a, int b)
{
   if(a < b)
      return a;
   else
      return b;
}
/* this file reads the filecontents and stores it in fileBuf*/
static int inline getContents(char* filename, int* fSize, int socket)
{
   
   FILE *f = fopen(filename, "rb");
   int tempSize = *fSize;
   int readSize;
   int bytesRead;
   int interval;
   int writeBytes;
   char fBuf[FILE_INTERVAL];
   if(f)
   {
      while(tempSize > 0)
      {
         interval   = min(tempSize, FILE_INTERVAL);
         bytesRead  = fread(fBuf, 1, interval,f);
         writeBytes = send(socket, fBuf, interval, 0);
         if(writeBytes < 0)
            error("Writing to socket");
         tempSize  -= interval;
      }
      /*if(fileBuf)
      {
        *fSize = fread(fileBuf, 1, *fSize, f);
      }*/
      fclose(f);
      return 1;
   }
   else
      return 0;
}

/*this file takes care of creating the HTTP response except for the file contents*/
static void inline writeResponse(char *response, const char *filename, file_type_t content_type, int* fSize )
{
   struct stat st;
   long size, tempSize, val, strSize;
   time_t lastModTime, curTime;
   int curDig , i , j, sizeOfSizeBuffer;
   char *sizeBuffer, *stringStack, *lastModTimeStr, *timeOfResponse;
   char lastModTimeStrArr[26];
   char timeOfResponseArr[26];
   lastModTimeStr = &lastModTimeStrArr[0];
   timeOfResponse = &timeOfResponseArr[0];
   char c[2];
   FILE *f = fopen(filename, "rb");
    

   if(f)
   {

      stat(filename, &st);
      size = (long) st.st_size; 
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
   timeOfResponse = asctime(localtime(&curTime));
   strcat(response, "Date: ");
   strcat(response, timeOfResponse);
   strcat(response, "Server: Apache/2.2.3 (CentOS)\r\n");

   if(f)
   {

      /* get last modified time and file size*/
      lastModTime = st.st_mtime;

      /* convert last modified time string */
      lastModTimeStr = ctime(&lastModTime);

      strcat(response, "Last-Modified: ");
      strcat(response, lastModTimeStr);
      strcat(response, "Content-Length: ");
      strcat(response, sizeBuffer);
      strcat(response, "\r\n");

      if(content_type == HTML)
         strcat(response, "Content-Type: text/html\r\n\r\n");
      else if(content_type == JPEG)
         strcat(response, "Content-Type: image/jpeg\r\n\r\n");
      else if(content_type == GIF)
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

   if(f) {
      free(sizeBuffer);
      free(stringStack);
   }
   //free(timeOfResponse);
}

int main(int argc, char *argv[])
{
   int sockfd, newsockfd, portno, pid, rc, maxfd;
   struct sockaddr_in server, client;
   socklen_t clientLen;   
   int end_server = 0;
   int i;
   http_header_t *header;

   fd_set active_fd_set, read_fd_set;
   if(argc < 2)
   {
      fprintf(stderr, "ERROR, no port provided\n");
   }
   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if(sockfd < 0)
      error("SOCKET");
   
   memset((char *) &server, 0, sizeof(server));
   portno = atoi(argv[1]);
   server.sin_family = AF_INET;
   server.sin_addr.s_addr = INADDR_ANY;
   server.sin_port=htons(portno);


   if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
      error("BIND ERROR");

   rc = listen(sockfd,10);

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
               int fSize;
               char div[3];
               div[0] = '\r';
               div[1] = '\n';
               div[2] = '\0';
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
                  printf("\n"
                         "%s\n"
                         , buffer);

                  // Clean up TCP connection
                  // Parse header
                  header = parse_header(buffer);
                  if (header && header->file_name) {
                     writeResponse(httpResponse, header->file_name, header->file_type,
                                   &fSize);
                  } else {
                     writeResponse(httpResponse, 0, UNKNOWN_FILE_TYPE,
                                   &fSize);
                  }
                 // fileBuf = (char *)malloc(fSize*sizeof(char));
                  writeBytes = send(i, httpResponse, strlen(httpResponse),0);
                  if(writeBytes < 0)
                     error("Error writing to socket");

                  getContents(header->file_name, &fSize,i);
                  //free(fileBuf);
                  free(header->file_name);
                  free(header);
                  close(i);
                  FD_CLR(i, &active_fd_set);
               }
            }
         }
      }
   }

 
   close(sockfd);

   return 0;

}
