#ifndef TINYCCHAT
#define TINYCCHAT

#ifndef PORT
#define PORT 8080
#endif // !PORT

// #include <sys/socket.h>
#include <netinet/in.h>

#define TC_IPV4            AF_INET
#define TC_ANY_ADDR        INADDR_ANY
#define TC_BD_SC_STREAM    SOCK_STREAM
#define TC_LISTEN_DEFAULT  SOMAXCONN
#define TC_LISTEN_CUSTOM   10
#define TC_MAX_MSG_LENGTH  1024
#define TC_MAX_NAME_LENGTH 25

#define TC_TEXT_RESET      "\033[0m"
#define TC_TEXT_BOLD       "\033[1m"
#define TC_TEXT_C_PROMPT   "\033[1;94m"

void tc_server_run(void);
void tc_client_connect(void);

#endif // !TINYCCHAT
