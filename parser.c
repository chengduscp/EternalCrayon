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
   exit(1);
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
   GIF,
   UNKNOWN
} file_type_t;

typedef enum accept_type_ {
   NO_ACCEPT_TYPE,
   ANYTHING,
   HTML_FILE,
   IMAGE_FILE
} accept_type_t;

typedef struct http_header_ {
   const char *file_name;
   file_type_t file_type;
   unsigned int host_ip, port;
   // We can ignore User-Agent
   accept_type_t accept_type;
   // We can ignore the other fields
   int keep_connection; // Boolean field
} http_header_t;


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
      if(!s->str) parser_error("Couldn't create memory for token");
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
            if (p->last != '\r' || next != '\n')
               parser_error("Line Ending Does not conform to standard");
            next = p->get(p->args); // Don't care about the newline anymore
            delete_string(t);
            t->c = '\n';
            t->type = LINEEND;
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
      else // Don't know what happened
         parser_error("Unknown Token");
   }
   p->last = next;
   return t;
}

// Parses the HTTP Header
int str_pos = 0;
int getFromString(void *arg)
{
   char *str = (char *)arg;
   if (str[str_pos] == '\0')
   {
      // printf("HAHA! I HAVE THWARTED YOU! >:D\n");
      return -1;
   }
   str_pos++;
   return (int)(str[str_pos - 1]);
}

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
      case UNKNOWN:
         printf("File type: UNKNOWN\n");
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
   if(!assertion) \
      parser_error("Unknown entity in header"); \
   delete_token(t); \

// Assumes we've just started parsing the header file and just gotten the 'GET'
char * get_filename(parser_t *p, file_type_t *type) {
   token_t *t;
   t = get_token(p);
   dy_str_t str;
   create_dy_str(&str);
   // Check for absolute path, store path, etc.
   // Have absolute path
   if (token_cmp_c(t, SEPERATOR, '/') == 0)
   {
      while(token_cmp(t, "HTTP") != 0)
      {
         if (token_cmp_c(t, SEPERATOR, '/') == 0)
         {
            add_c(&str, '/');
         }
         else if (t->type == TOKEN)
         {
            add_str(&str, t->string.str);
         }
         delete_token(t);
         t = get_token(p);
      }
   }
   delete_token(t);

   // Get file type information
   char *c = &str.str[str.len-1];

   // Get the file extention
   if (str.len > 1)
      while(*(c-1) != '.' && *(c-1) != '/') c--;

   // Add appropriate file type
   if (strcmp(c, "/") == 0) {
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
      // For now, we just use plain text if we don't know the format type
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
   TOKEN_ASSERTION(t, p, (token_cmp(t, "Host") == 0));
   TOKEN_ASSERTION(t, p, (token_cmp_c(t, SEPERATOR, ':') == 0));

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
   TOKEN_ASSERTION(t, p, (token_cmp_c(t, SEPERATOR, ':') == 0));
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
   h->port = port;
   TOKEN_ASSERTION(t, p, (t->type == LINEEND));
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
         TOKEN_ASSERTION(t, p, (token_cmp_c(t, SEPERATOR, ':') == 0));
         t = get_token(p);
         if (token_cmp(t, "keep-alive") == 0) {
            h->keep_connection = 1;
            // printf("Kept connection\n");
         } else {
            h->keep_connection = 0;
            // printf("Did not keep connection\n");
         }
      } else {
         // We also just skip this for now
         // parser_error("Unknown sub-header type");
      }
      while (t->type != LINEEND) {
         delete_token(t);
         t = get_token(p);
      }
      delete_token(t);
      token_t *t = get_token(p);
   }
}

http_header_t *get_header(parser_t *p) {
   http_header_t *h = (http_header_t *)malloc(sizeof(http_header_t));

   h->file_name = get_filename(p, &h->file_type);
   get_port(p, h);
   get_sub_headers(p, h);

   return h;
}

void parse_header(const char *header)
{
   token_t *t;
   parser_t p;
   p.get = &getFromString;
   p.args = (void*)header;
   p.last = BEGINNING_OF_FILE; // initialize as BEGINNING_OF_FILE
   // Get the Request type
   t = get_token(&p);
   if (token_cmp(t, "GET") == 0)
   {
      http_header_t *header = get_header(&p);
      printf("%s\n", header->file_name);
      print_file_type(&header->file_type);
      printf("%x:%u\n", header->host_ip, header->port);
   }
   else if (error_token(t))
   {
      printf("Awww :(\n");
      printf("Error parsing header\n");
      print_t(t);
   }
   else
   {
      printf("Unsupported method type\n");
   }
   delete_token(t);
}

char header[] = 
   "GET / HTTP/1.1\r\n"
   "Host: 127.0.0.1:9999\r\n"
   "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux i686; rv:24.0) Gecko/20100101 Firefox/24.0\r\n"
   "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
   "Accept-Language: en-US,en;q=0.5\r\n"
   "Accept-Encoding: gzip, deflate\r\n"
   "Connection: keep-alive\r\n"
   "\r\n";

int main(int argc, char const *argv[])
{
   while(0/*argc*/)
   {
      printf("%s\n", "hi");
      printParse(argv[0]);
      str_pos = 0;
      argc--; argv++;
   }
   printf("(HEADER) ---------------- \n");
   // printParse(header);
   str_pos = 0;
   parse_header(header);
   return 0;
}

