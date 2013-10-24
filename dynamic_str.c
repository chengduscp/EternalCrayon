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

