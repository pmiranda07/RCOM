#include "URL.h"

void default_auth(url_info* url_info){
  memcpy(url_info->user, "anonymous", strlen("anonymous") + 1);
  memcpy(url_info->password, "mail@domain", strlen("mail@domain") + 1);
};

int init_auth(url_info* url_info, char* url, char* at_position){
  char* slash = strchr(url, '/'); //slash is never null
  slash += 2;
  char* password = strchr(slash, ':');
  if(password == NULL){
    fprintf(stderr, "Your link must contain a ':' separating the username and password!'\n");
    return 1;
  }
  memcpy(url_info->user, slash, password - slash);
  url_info->user[password-slash]=0;
  password++;
  memcpy(url_info->password, password, at_position-password);
  url_info->password[at_position-password] = 0;
  return 0;
}

int parse_url(char url[], url_info* url_info){
  if(strncmp(url, "ftp://", strlen("ftp://")) != 0){
    fprintf(stderr, "Your link must begin with 'ftp://'\n");
    return 1;
  }
  char* at_position = strrchr(url, '@');
  if(at_position == NULL){
    default_auth(url_info);
    at_position = url + strlen("ftp://");
  }
  else{
    if(init_auth(url_info, url, at_position) != 0)
      return 1;
    at_position++;
  }

  char* first_slash = strchr(at_position, '/');
  memcpy(url_info->host_url, at_position, first_slash-at_position);
  url_info->host_url[first_slash-at_position] = 0;

  char* last_slash = strrchr(url, '/');
  last_slash++;
  memcpy(url_info->file_path, first_slash, last_slash-first_slash);
  url_info->file_path[last_slash-first_slash] = 0;

  memcpy(url_info->filename, last_slash, strlen(last_slash) + 1);
  //getip
  if ((url_info->host_info=gethostbyname(url_info->host_url)) == NULL) {
      herror(url_info->host_url);
      exit(1);
  }

  return 0;
}
