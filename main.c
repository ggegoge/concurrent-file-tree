#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "Tree.h"

void simple_tree_test()
{
  printf("SIMPLE TREE TEST\n");
  Tree* root = tree_new();
  int e1 = tree_create(root, "/a/");
  int e2 = tree_create(root, "/b/");

  if (e1 || e2)
    printf("e1 = %d, e2 = %d\n", e1, e2);

  char* listing = tree_list(root, "/");
  printf("\t%s\n", listing);
  free(listing);
  int e3 = tree_create(root, "/a/b/");
  int e4 = tree_create(root, "/a/c/");
  int e5 = tree_create(root, "/a/wyjebany/");

  listing = tree_list(root, "/a/");
  printf("\t%s\n", listing);
  free(listing);

  int e6 = tree_move(root, "/a/", "/b/chuuj/");

  if (e3 || e4 || e5 || e6)
    printf("e3 = %d, e4 = %d, e5 = %d\n", e3, e4, e5);

  listing = tree_list(root, "/");
  printf("\t%s\n", listing);
  free(listing);

  listing = tree_list(root, "/a/");
  printf("\t%s\n", listing);
  free(listing);

  listing = tree_list(root, "/b/");
  printf("\t%s\n", listing);
  free(listing);

  listing = tree_list(root, "/b/chuuj/");
  printf("\t%s\n", listing);
  free(listing);

  int e7 = tree_remove(root, "/b/chuuj/wyjebany/");

  if (e7)
    printf("e7 = %d\n", e7);

  listing = tree_list(root, "/b/chuuj/");
  printf("\t%s\n", listing);
  free(listing);

  tree_free(root);
}

int main(void)
{
  simple_tree_test();
  return 0;
}
