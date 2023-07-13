#include "./src/tinyCchat.h"

int main(void)
{
  const char *db_name_example = "file.db";
  tc_server_run(db_name_example);
  return 0;
}
