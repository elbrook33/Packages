#ifndef UTILITIES_H
#define UTILITIES_H

#include "List.h"
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

const char* join(const char** array) {
  int l = 0;
  for(const char** pass1=array; *pass1; pass1++) {
    l += strlen(*pass1);
  }
  char* result = init_string(l+1);
  result[0] = '\0';
  for(const char** pass2=array; *pass2; pass2++) {
    strcat(result, *pass2);
  }
  return result;
}

const char* first_term(const char* path) {
  char* result = init_string(strlen(path));
  for (int i=0; path[i]; i++) {
    if(path[i] == '/') return result;
    result[i] = path[i];
  }
  return result;
}

const char* last_term(const char* path) {
  int l=strlen(path), i=l-1;
  char* reverse = init_string(l);
  for (; i>=0; i--) {
    if(path[i] == '/') break;
    reverse[l-1-i] = path[i];
  }
  for (int j=0; j<(l-1-i)/2; j++) {
    char tmp = reverse[j];
    reverse[j] = reverse[l-1-i-1-j];
    reverse[l-1-i-1-j] = tmp;
  }
  return reverse;
}

bool ignore_file(char* name) {
  return name[0] == '.' && (name[1] == '\0' || name[1] == '.');
}

List* ls(const char* path) {
  DIR *dp = opendir(path);
  struct dirent *de;
  if (dp == NULL) return NULL;
  
  List* L = List_new();
  while ((de = readdir(dp)) != NULL) {
    if(!ignore_file(de->d_name)) List_add(L, de->d_name);
  }
  closedir(dp);
  
  return L;
}

List* ls_fullpath(const char* path) {
  List* L = List_new();

  DIR *dp = opendir(path);
  struct dirent *de;
  if (dp == NULL) return L;
  
  while ((de = readdir(dp)) != NULL) {
    if(ignore_file(de->d_name)) continue;
    const char* array[4] = {path, "/", de->d_name};
    const char* fullpath = join(array);
    List_add(L, fullpath);
    free((void*)fullpath);
  }
  closedir(dp);
  
  return L;
}

#endif
