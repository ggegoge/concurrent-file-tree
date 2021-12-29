#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "Tree.h"

#define ITER 100

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

  int e8 = tree_move(root, "/b/chuuj/", "/b/chuuj/c/x/");
  assert(e8);

  tree_free(root);
}

/* erroneous commands and checking whether correct errnos are returned
 * TODO */
void errors_tree_test()
{

}

/* THREADED TESTS */
void* move_tester1(void* tree)
{
  for (int i = 0; i < ITER; i++) {
    tree_move((Tree*)tree, "/a/b/", "/b/x/");
    tree_move((Tree*)tree, "/b/x/", "/a/b/");
  }

  return NULL;
}

void* move_tester2(void* tree)
{
  for (int i = 0; i < ITER; i++) {
    tree_move((Tree*)tree, "/b/x/", "/a/b/");
    tree_move((Tree*)tree, "/a/b/", "/b/x/");
  }

  return NULL;
}

void move_test_async()
{
  Tree* tree = tree_new();

  printf("move_test_sync\n");

  tree_create(tree, "/a/");
  tree_create(tree, "/b/");
  tree_create(tree, "/a/b/");
  tree_create(tree, "/a/b/c/");
  tree_create(tree, "/a/b/d/");
  tree_create(tree, "/b/a/");
  tree_create(tree, "/b/a/d/");

  pthread_t t[2];
  pthread_create(&t[0], NULL, move_tester1, tree);
  pthread_create(&t[1], NULL, move_tester2, tree);

  pthread_join(t[0], NULL);
  pthread_join(t[1], NULL);

  tree_tree(tree, 0);

  tree_free(tree);
}


int main(void)
{
  simple_tree_test();
  errors_tree_test();
  move_test_async();

  return 0;
}
