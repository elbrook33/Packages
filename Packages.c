/*
  Packages.c
  ==========
  An organiser of package installations.
  
  * When looking for files, combines user's package directory with system packages.
  * When installing files, puts every batch of new files in its own folder.
  
  To do:
  ------
  * Clarify duplicates. Right now it lists both -- even if they have the same name -- and repeats one of them twice when reading (chosen mostly at random).
  * Caching. Don't search through every sub-directory every time.
*/

// Custom helpers
#include "OverlayFS.h"
#include "Utilities.h"
#include "List.h"
// System libraries
#include <fuse.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

//
// Settings
//
#define userDir "/home/"
#define saveDir "/Packages/"
#define rootDir "/Computer/System/Packages/"
#define newName "Unsorted-"
#define NewPackageInterval 30
static int dailyCounter = 0;
static time_t lastCreate = 0;

//
const char* UserDir() {
  uid_t uid = fuse_get_context()->uid;
  if(uid > 0) {
    char* user = getpwuid(uid)->pw_name;
    const char* array[4] = {userDir, user, saveDir, NULL};
    return join(array);
  } else {
    return NULL;
}}

//
// Reading files
//
const char* FindActualFile(const char* base, const char* path) {
  List* Packages = ls(base);
  for(int i=0; i<Packages->length; i++) {
    const char* array[5] = {base, Packages->str[i], "/", path, NULL};
    const char* subPath = join(array);
    if(access(subPath, F_OK) != -1) {
      List_free(Packages);
      return subPath;
  }}
  List_free(Packages);
  return NULL;
}
const char* Redirect(const char* path) {
  const char* userPath = UserDir();
  const char* result;
  if(userPath != NULL) {
    result = FindActualFile(userPath, path);
    free((void*)userPath);
  }
  if(userPath == NULL || result == NULL) {
    result = FindActualFile(rootDir, path);
  }
  return result;
}

//
// Creating new files
//
const char* Redirect_Create(const char* path) {
  const char* dir = UserDir();
  if(dir == NULL) dir = rootDir;
  bool createPath = false;
  if(time(NULL)-lastCreate > NewPackageInterval) {
    dailyCounter++;
    lastCreate = time(NULL);
    createPath = true;
  }
  char basePath[64];
  sprintf(basePath, "%s%s%i/", dir, newName, dailyCounter);
  free((void*)dir);
  if(createPath) mkdir(basePath, 0755);
  const char* array[3] = {basePath, path, NULL};
  const char* newPath = join(array);
  return newPath;
}

//
// Listing directories
//
List* ListFiles(const char* base, const char* path) {
  List* Packages = ls(base);
  List* SubDirs[Packages->length+1];
  SubDirs[Packages->length] = NULL;
  for(int i=0; i<Packages->length; i++) {
    const char* array[5] = {base, Packages->str[i], "/", path, NULL};
    const char* subPath = join(array);
    SubDirs[i] = ls_fullpath(subPath);
    free((void*)subPath);
  }
  free((void*)Packages);
  return join_lists(SubDirs);
}
int Redirect_List(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* file_info) {
  const char* userPath = UserDir();
  List* L;
  if(userPath == NULL) {
    L = ListFiles(rootDir, path);
  } else {
    List* lists[3];
    lists[0] = ListFiles(userPath, path);
    lists[1] = ListFiles(rootDir, path);
    lists[2] = NULL;
    free((void*)userPath);
    L = join_lists(lists);
  } 
  for (int i=0; i<L->length; i++) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    if(stat(L->str[i], &st) == -1) {
      List_free(L);
      return -errno;
    }
    const char* name = last_term(L->str[i]);
    int result = filler(buffer, name, &st, 0);
    free((void*)name);
    if(result != 0) break;
  }
  List_free(L);
  return 0;
}

//
void PathFreer(const char* path) {
  free((void*)path);
}

//
int main(int argc, char* argv[]) {
  overlay_path_modifier       = Redirect;
  overlay_path_creator        = Redirect_Create;
  overlay_path_freer          = PathFreer;
  overlay_operations.readdir  = Redirect_List;
  return fuse_main(argc, argv, &overlay_operations, NULL);
}
