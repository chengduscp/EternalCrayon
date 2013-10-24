
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

