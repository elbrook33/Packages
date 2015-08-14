#ifndef LIST_H
#define LIST_H

#include <string.h>
#include <stdlib.h>

char* init_string(int length) {
  int size = length*sizeof(char);
  char* str = malloc(size);
  memset(str, '\0', size);
  return str;
}
char* copy_string(const char* str) {
  int N = strlen(str);
  char* result = init_string(N+1);
  result[N] = '\0';
  return strcpy(result, str);
}

#define LIST_N 64

typedef struct List {
  int size;
  int length;
  char** str;
} List;

List* List_new_N(int N) {
  List* L = malloc(sizeof(List));
  L->str = malloc(N*sizeof(char*));
  L->size = N;
  L->length = 0;
  return L;
}
List* List_new() {
  return List_new_N(LIST_N);
}
void List_free_elements(List* L) {
  for(int i=0; i<L->length; i++) {
    free(L->str[i]);
  }
}
void List_free(List* L) {
  List_free_elements(L);
  free(L->str);
  free(L);
}
void List_add(List* L, const char* str) {
  if(L->length == L->size) {
    L->size += LIST_N;
    L->str = realloc(L->str, L->size*sizeof(char*));
  }
  L->str[L->length] = copy_string(str);
  L->length++;
}

List* join_lists(List** array) {
  int N = 0;
  for (List** pass1=array; *pass1; pass1++) {
    N += (*pass1)->length;
  }
  List* results = List_new_N(N);
  for (List** pass2=array; *pass2; pass2++) {
    for(int j=0; j<(*pass2)->length; j++) {
      List_add(results, (*pass2)->str[j]);
    }
    List_free(*pass2);
  }
  return results;
}
#endif
