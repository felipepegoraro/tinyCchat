#include "./tinyCchat.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <sqlite3.h>


void tc_handle_error(const char *msg_err)
{
  if (errno) perror(strerror(errno));
  else fprintf(stderr, "TC ERROR: %s\n", msg_err);
  exit(EXIT_FAILURE);
}


sqlite3 *tc_database_connection(const char *database_name)
{
  sqlite3 *db;
  int tc_res = sqlite3_open(database_name, &db);
  if (tc_res != SQLITE_OK)
  {
    sqlite3_close(db);
    tc_handle_error("database connection");
  }

  return db;
}


void tc_database_create_table(sqlite3 *db)
{
  const char *query  = 
    "CREATE TABLE users ("
     "id INTEGER PRIMARY KEY, "
     "name TEXT, "
     "mensagens TEXT[]"
     ")";

  int tc_res = sqlite3_exec(db, query, NULL, 0, NULL);
  if (tc_res != SQLITE_OK)
  {
    sqlite3_close(db);
    tc_handle_error("table creation");
  }
}


void tc_database_send_msg(
  sqlite3 *db,
  const char *msg,
  size_t size_msg,
  const char *user,
  size_t user_len
) {
  sqlite3_stmt *stmt;
  const char *query = "INSERT INTO users (name, mensagens) VALUES (?, ?)";

  int tc_res = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
  if (tc_res != SQLITE_OK) tc_handle_error("prepare statement");

  tc_res = sqlite3_bind_text(stmt, 1, user, user_len, SQLITE_STATIC);
  if (tc_res != SQLITE_OK) tc_handle_error("bind user parameter");

  tc_res = sqlite3_bind_text(stmt, 2, msg, size_msg, SQLITE_STATIC);
  if (tc_res != SQLITE_OK) tc_handle_error("bind message parameter");

  tc_res = sqlite3_step(stmt);
  if (tc_res != SQLITE_DONE) tc_handle_error("execute statement");

  tc_res = sqlite3_reset(stmt);
  if (tc_res != SQLITE_OK) tc_handle_error("reset statement");

  tc_res = sqlite3_finalize(stmt);
  if (tc_res != SQLITE_OK) tc_handle_error("finalize statement");
}


void tc_name_handler(char *name, size_t size)
{
  for (size_t i=0; i<size; i++){
    if (name[i] == ' '){
      name[i] = '_';
    }
  }
}



void tc_read_username(char *username, size_t size)
{
  printf("Enter your name: ");
  fgets(username, size, stdin);
  tc_name_handler(username, size);
  username[strcspn(username, "\n")] = '\0';
}



void tc_send_connected_user(const char *username, int fd_server)
{
  char msg_to_server[TC_MAX_MSG_LENGTH];
  sprintf(msg_to_server, "user %s connected\n", username);
  int tc_res = send(fd_server, msg_to_server, strlen(msg_to_server), 0);
  if (tc_res < 0) tc_handle_error("while sending the message");
  printf("Welcome, %s!\nPress enter to start the chat.\n", username);
}



int tc_receive_msg_from_server(int fd_server, fd_set *tmp_fds)
{
  if (FD_ISSET(fd_server, tmp_fds))
  {
    char buffer[TC_MAX_MSG_LENGTH];
    int recv_size = recv(fd_server, buffer, sizeof(buffer), 0);

    if (recv_size < 0) tc_handle_error("while receiving the message from the server");
    else if (recv_size == 0){
      printf("connection with the server closed\n");
      return 1;
    } else {
      buffer[recv_size] = '\0';
      printf("%s\n", buffer);
    }
  }
  return 0;
}


int tc_users_send_msg_to_server(int fd_server, fd_set *tmp_fds)
{
  int tc_res;
  if (FD_ISSET(0, tmp_fds))
  {
    char message[TC_MAX_MSG_LENGTH];
    fgets(message, sizeof(message), stdin);
    message[strcspn(message, "\n")] = '\0';

    tc_res = send(fd_server, message, strlen(message), 0);
    if (tc_res < 0) tc_handle_error("message sending");

    if (strcmp(message, "exit") == 0) return 1;
  }
  return 0;
}



void tc_server_create(int *fd_server)
{
  int tc_res = 0;

  struct sockaddr_in address;
  int addr_l = sizeof(address);

  *fd_server = socket(TC_IPV4, TC_BD_SC_STREAM, 0);
  if (*fd_server == 0) tc_handle_error("socket creation\n");

  address.sin_family      = TC_IPV4;
  address.sin_addr.s_addr = TC_ANY_ADDR;
  address.sin_port        = htons(PORT);

  tc_res = bind(*fd_server, (struct sockaddr*)&address, addr_l);
  if (tc_res < 0) tc_handle_error("binding socket");

  tc_res = listen(*fd_server, TC_LISTEN_DEFAULT);
  if (tc_res < 0) tc_handle_error("at listening");
}



void tc_server_close(const char *msg_finish, int fd_server)
{
  printf("TC: %s\n", msg_finish);
  close(fd_server);
}



void tc_client_connect(void)
{
  int fd_server;
  struct sockaddr_in server_address;

  fd_server = socket(TC_IPV4, TC_BD_SC_STREAM, 0);
  if (fd_server == 0) tc_handle_error("at socket creation");

  server_address.sin_family = TC_IPV4;
  server_address.sin_port = htons(PORT);

  int tc_res = inet_pton(TC_IPV4, "127.0.0.1", &server_address.sin_addr);
  if (tc_res <= 0) tc_handle_error("conversion of the IP address");

  tc_res = connect(fd_server, (struct sockaddr *)&server_address, sizeof(server_address));
  if (tc_res < 0) tc_handle_error("server connection");

  char username[TC_MAX_NAME_LENGTH];
  tc_read_username(username, sizeof(username));
  tc_send_connected_user(username, fd_server);

  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(0, &read_fds); // stdin
  FD_SET(fd_server, &read_fds);

  int max_fd = (fd_server > 0) ? fd_server : 0;

  int tc_receive = 0, tc_send = 0;

  while (tc_receive == 0 && tc_send == 0)
  {
    fd_set tmp_fds = read_fds;
    tc_res = select(max_fd + 1, &tmp_fds, NULL, NULL, NULL);
    if (tc_res < 0) tc_handle_error("at select");

    tc_receive = tc_receive_msg_from_server(fd_server,  &tmp_fds);
    tc_send    = tc_users_send_msg_to_server(fd_server, &tmp_fds);

    printf("%s%s> %s", TC_TEXT_C_PROMPT, username, TC_TEXT_RESET);
    fflush(stdout);
  };

  close(fd_server);
}



void tc_initialize_client_sockets(int *client_sockets, char usernames[][TC_MAX_NAME_LENGTH])
{
  for (int i = 0; i < TC_LISTEN_DEFAULT; i++)
  {
    client_sockets[i] = 0;
    usernames[i][0] = '\0';
  }
}



void tc_add_server_socket_to_fd_set(int fd_server, fd_set *read_fds, int *max_fd)
{
  FD_SET(fd_server, read_fds);
  if (fd_server > *max_fd)
    *max_fd = fd_server;
}



void tc_add_client_sockets_to_fd_set(int* client_sockets, fd_set *read_fds, int *max_fd)
{
  for (int i = 0; i < TC_LISTEN_DEFAULT; i++)
  {
    int client_socket = client_sockets[i];

    if (client_socket > 0)
    {
      FD_SET(client_socket, read_fds);
      if (client_socket > *max_fd)
        *max_fd = client_socket;
    }
  }
}



void tc_handle_new_connections(
  int fd_server,
  int *client_sockets,
  char usernames[][TC_MAX_NAME_LENGTH]
){
  struct sockaddr_in address;
  int addrlen = sizeof(address);

  int new_socket = accept(fd_server, (struct sockaddr*)&address, (socklen_t*)&addrlen);
  if (new_socket < 0) tc_handle_error("connection not accepted");

  for (int i = 0; i < TC_LISTEN_DEFAULT; i++)
  {
    if (client_sockets[i] == 0)
    {
      client_sockets[i] = new_socket;
      printf("new connection accepted on socket %d\n", new_socket);

      char connected_user[TC_MAX_MSG_LENGTH];
      int recv_size = recv(new_socket, connected_user, sizeof(connected_user), 0);

      if (recv_size < 0) tc_handle_error("receive username");
      else if (recv_size == 0)
      {
        close(new_socket);
        continue;
      }

      connected_user[recv_size] = '\0';
      char username[TC_MAX_MSG_LENGTH];
      sscanf(connected_user, "user %s connected\n", username);
      printf("new client connected: %s\n", username);

      strncpy(usernames[i], username, TC_MAX_NAME_LENGTH - 1);
      usernames[i][TC_MAX_NAME_LENGTH - 1] = '\0';

      break;
    }
  }
}



void tc_handle_client_data(
  sqlite3 *db,
  int *client_sockets,
  fd_set *read_fds,
  char usernames[][TC_MAX_NAME_LENGTH]
){
  for (int i = 0; i < TC_LISTEN_DEFAULT; i++)
  {
    int client_socket = client_sockets[i];

    if (FD_ISSET(client_socket, read_fds))
    {
      char buffer[TC_MAX_MSG_LENGTH];
      int recv_size = recv(client_socket, buffer, sizeof(buffer), 0);

      if (recv_size < 0)
      {
        tc_handle_error("when receiving the message from the client");
      }
      else if (recv_size == 0)
      {
        printf("client disconnected: %s\n", usernames[i]);
        close(client_socket);
        client_sockets[i] = 0;
        usernames[i][0] = '\0';
      }
      else
      {
        buffer[recv_size] = '\0';
        printf("[%s]: %s\n", usernames[i], buffer);

        for (int j = 0; j < TC_LISTEN_DEFAULT;j++)
        {
          int dest_socket = client_sockets[j];

          if (dest_socket != client_socket && dest_socket > 0)
          {
            char formatted_message[TC_MAX_MSG_LENGTH + TC_MAX_NAME_LENGTH + 4];
            sprintf(formatted_message, "\n%s> %s", usernames[i], buffer);

            size_t len_user = strlen(usernames[i]);
            size_t len_buff = strlen(buffer);
            tc_database_send_msg(db, buffer, len_buff, usernames[i], len_user);

            int res = send(dest_socket, formatted_message, strlen(formatted_message), 0);
            if (res < 0) tc_handle_error("message send");
          }
        }
      }
    }
  }
}



void tc_server_run(void)
{
  sqlite3 *db = tc_database_connection("file.db");
  tc_database_create_table(db);

  int fd_server;
  // struct sockaddr_in address;
  // int addrlen = sizeof(address);

  fd_set read_fds;
  int client_sockets[TC_LISTEN_DEFAULT];
  char usernames[TC_LISTEN_DEFAULT][TC_MAX_NAME_LENGTH];
  int max_fd;
  int tc_res; // activity

  tc_server_create(&fd_server);
  tc_initialize_client_sockets(client_sockets, usernames);

  while (1)
  {
    FD_ZERO(&read_fds);

    tc_add_server_socket_to_fd_set(fd_server, &read_fds, &max_fd);
    tc_add_client_sockets_to_fd_set(client_sockets, &read_fds, &max_fd);

    tc_res = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
    if (tc_res < 0) tc_handle_error("select multiplexing");

    if (FD_ISSET(fd_server, &read_fds)){
      tc_handle_new_connections(fd_server, client_sockets, usernames);
    }

    tc_handle_client_data(db, client_sockets, &read_fds, usernames);
  }

  tc_server_close("server closed", fd_server);
}
