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

