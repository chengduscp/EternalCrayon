#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/*
NEXT:
- Make a simple dynamic string library with add character and add string functions
*/


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

// Fundamental unit of the parser
typedef struct token_ {
   token_type_t type;
   unsigned len; // Length of string (not including null character)
   unsigned cap; // Capacity of str's buffer
   char *str; // Actual characters (null terminated)
} token_t;

// Parser to read the HTTP Header
typedef struct parser_ {
   int (*get)(void*); // Get next character
   void *args; // Args for get
   int last; // Last drawn character
} parser_t;

// Basic token functions
//############################################################
token_t* create_token()
{
   static const int INITIAL_SIZE = 8;
   token_t *t = (token_t *)malloc(sizeof(token_t));
   t->str = (char *)malloc(INITIAL_SIZE);
   t->cap = INITIAL_SIZE;
   t->len = 0;
   t->type = NONE;
   return t;
}

// Printout for debugging
void print(token_t *t)
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
         printf("Regular token: %s\n", t->str);
         break;
      case SEPERATOR:
         printf("Seperator token: %c\n", t->str[0]);
         break;
      case LINEEND:
         printf("Line end token: \\r\\n\n");
         break;
      case TERMINATE:
         printf("End of file token\n");
         break;
   }
}
// Add character to the token string
void add(token_t *t, char c)
{
   if (t->len + 1 >= t->cap)
   {
      t->cap *= 2;
      t->str = realloc(t->str, t->cap);
      if(!t->str) parser_error("Couldn't create memory for token");
   }
   t->str[t->len] = c;
   t->str[t->len + 1] = '\0';
   t->len++;
}

void delete_string(token_t *t)
{
   if (!t) return;
   if(t->str) free(t->str);
   t->str = 0;
   t->len = 0;
   t->cap = 0;
}

void delete_token(token_t *t)
{
   if(!t) return;
   delete_string(t);
   free(t);
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
   if(t->len == 0)
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
            t->type = LINEEND;
         }
      }
      else if (next < 0) // End of File
      {
         delete_string(t);
         t->type = TERMINATE;
      }
      else if (is_seperator(next)) // Separator token
      {
         char c = (char)next;
         add(t, c);
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

void printParse(const char *c_str)
{
   token_t *t = NULL;
   parser_t p;
   p.get = &getFromString;
   p.args = (void*)c_str;
   p.last = BEGINNING_OF_FILE; // initialize as BEGINNING_OF_FILE
   while ((t = get_token(&p)))
   {
      print(t);
      if (t->type == TERMINATE)
         break;
      delete_token(t);
   }
   // Delete the last token
   delete_token(t);
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
   if (t->str && strcmp(t->str, "GET") == 0)
   {
      printf("HAHA I HAVE PARSED SUCCESSFULLY >:D\n");
      delete_token(t);
      t = get_token(&p);
      print(t);
      // Check for absolute path, store path, etc.
   }
   else if (!t || !t->str)
   {
      printf("Awww :(\n");
      print(t);
   }


}

char header[] = 
   "GET / HTTP/1.1\r\n"
   "Host: 127.0.0.1:9999\r\n"
   "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux i686; rv:24.0) Gecko/20100101 Firefox/24.0\r\n"
   "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
   "Accept-Language: en-US,en;q=0.5\r\n"
   "Accept-Encoding: gzip, deflate\r\n"
   "Connection: keep-alive\r\n";

int main(int argc, char const *argv[])
{
   while(argc)
   {
      printf("%s\n", "hi");
      printParse(argv[0]);
      str_pos = 0;
      argc--; argv++;
   }
   printf("(HEADER) ---------------- \n");
   printParse(header);
   str_pos = 0;
   parse_header(header);
   return 0;
}

