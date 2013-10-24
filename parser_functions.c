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