#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Tree.h"

void simple_tree_test()
{
  printf("SIMPLE TREE TEST\n");
  Tree* root = tree_new();
  int e1 = tree_create(root, "/a/");
  int e2 = tree_create(root, "/b/");

  assert(!e1 && !e2);

  char* listing = tree_list(root, "/");
  assert(strcmp(listing, "a,b") == 0);
  printf("\t%s\n", listing);
  free(listing);
  int e3 = tree_create(root, "/a/b/");
  int e4 = tree_create(root, "/a/c/");
  int e5 = tree_create(root, "/a/wyjebany/");

  listing = tree_list(root, "/a/");
  assert(strcmp(listing, "b,c,wyjebany") == 0);
  printf("\t%s\n", listing);
  free(listing);

  int e6 = tree_move(root, "/a/", "/b/chuuj/");

  assert(!e3 && !e4 && !e5 && !e6);

  listing = tree_list(root, "/");
  assert(strcmp(listing, "b") == 0);
  printf("\t%s\n", listing);
  free(listing);

  listing = tree_list(root, "/a/");
  assert(!listing);
  printf("\t%s\n", listing);
  free(listing);

  listing = tree_list(root, "/b/");
  assert(strcmp(listing, "chuuj") == 0);
  printf("\t%s\n", listing);
  free(listing);

  listing = tree_list(root, "/b/chuuj/");
  assert(strcmp(listing, "b,c,wyjebany") == 0);
  printf("\t%s\n", listing);
  free(listing);

  int e7 = tree_remove(root, "/b/chuuj/wyjebany/");

  assert(!e7);
  
  listing = tree_list(root, "/b/chuuj/");
  assert(strcmp(listing, "b,c") == 0);
  printf("\t%s\n", listing);
  free(listing);

  tree_free(root);
}

/* erroneous commands and checking whether correct errnos are returned
 * TODO */
void errors_tree_test()
{

}

int main(void)
{
  simple_tree_test();
  errors_tree_test();
  return 0;
}
