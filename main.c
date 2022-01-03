#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "Tree.h"
#include "path_utils.h"

#define ITER 100
#define N 100

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

  tree_free(tree);
}

void* creator(void* tree)
{
  for (int k = 0; k < 20; k++) {
    char path[12];
    int n = rand() % 3 + 1;

    for (int i = 0; i < n; i++) {
      path[2 * i] = '/';
      path[2 * i + 1] = 'a' + rand() % 4;
    }

    path[2 * n] = '/';
    path[2 * n + 1] = '\0';

    //printf("%s\n", path);
    tree_create((Tree*)tree, path);
  }

  return NULL;
}

void* remover(void* tree)
{
  for (int k = 0; k < 20; k++) {
    char path[12];
    int n = rand() % 3 + 1;

    for (int i = 0; i < n; i++) {
      path[2 * i] = '/';
      path[2 * i + 1] = 'a' + rand() % 4;
    }

    path[2 * n] = '/';
    path[2 * n + 1] = '\0';
    tree_remove((Tree*)tree, path);
  }

  return NULL;
}

void* lister(void* tree)
{
  for (int k = 0; k < 20; k++) {
    char path[12];
    int n = rand() % 4;

    for (int i = 0; i < n; i++) {
      path[2 * i] = '/';
      path[2 * i + 1] = 'a' + rand() % 4;
    }

    path[2 * n] = '/';
    path[2 * n + 1] = '\0';
    char* list = tree_list((Tree*)tree, path);

    if (list) {
      //pthread_mutex_lock(&printf_mutex);
      //printf("%s\n", list);
      //pthread_mutex_unlock(&printf_mutex);
      free(list);
    }
  }

  return NULL;
}

void* mover(void* tree)
{
  for (int k = 0; k < 20; k++) {
    char path_s[12];
    char path_t[12];
    int n_s = rand() % 3 + 1;
    int n_t = rand() % 4;

    for (int i = 0; i < n_s; i++) {
      path_s[2 * i] = '/';
      path_s[2 * i + 1] = 'a' + rand() % 4;
    }

    path_s[2 * n_s] = '/';
    path_s[2 * n_s + 1] = '\0';

    for (int i = 0; i < n_t; i++) {
      path_t[2 * i] = '/';
      path_t[2 * i + 1] = 'a' + rand() % 4;
    }

    path_t[2 * n_t] = '/';
    path_t[2 * n_t + 1] = '\0';

    tree_move((Tree*)tree, path_s, path_t);
  }

  return NULL;
}

void test2()
{
  Tree* tree = tree_new();

  printf("ALEATORY THREADS TEST\n");
  
  //pthread_mutex_init(&printf_mutex, NULL);

  srand(2137);

  pthread_t c[N], r[N], m[N], l[N];

  for (int i = 0; i < N; i++) {
    pthread_create(&c[i], NULL, &creator, tree);
    pthread_create(&r[i], NULL, &remover, tree);
    pthread_create(&m[i], NULL, &mover, tree);
    pthread_create(&l[i], NULL, &lister, tree);
  }

  for (int i = 0; i < N; i++) {
    pthread_join(c[i], NULL);
  }

  for (int i = 0; i < N; i++) {
    pthread_join(r[i], NULL);
  }

  for (int i = 0; i < N; i++) {
    pthread_join(m[i], NULL);
  }

  for (int i = 0; i < N; i++) {
    pthread_join(l[i], NULL);
  }

  //pthread_mutex_destroy(&printf_mutex);

  //print_tree(tree);

  tree_free(tree);
}

void test_lca()
{
  char* p1 = "/a/b/c/d/";
  char* p2 = "/a/b/x/y/";
  const char* p1lca;
  const char* p2lca;  
  char* lca_path = path_lca_move(p1, p2, &p1lca, &p2lca);
  assert(is_path_valid(p1));
  assert(is_path_valid(p2));
  printf("common_path = %s, restings=%s and %s\n", lca_path, p1lca, p2lca);
  free(lca_path);

  p1 = "/";
  p2 = "/b/";
  lca_path = path_lca_move(p1, p2, &p1lca, &p2lca);
  assert(is_path_valid(p1));
  assert(is_path_valid(p2));
  printf("common_path = %s, restings=%s and %s\n", lca_path, p1lca, p2lca);
  free(lca_path);


  p1 = "/x/";
  p2 = "/b/";
  lca_path = path_lca_move(p1, p2, &p1lca, &p2lca);
  assert(is_path_valid(p1));
  assert(is_path_valid(p2));
  printf("common_path = %s, restings=%s and %s\n", lca_path, p1lca, p2lca);
  free(lca_path);

  p1 = "/x/";
  p2 = "/x/";
  lca_path = path_lca_move(p1, p2, &p1lca, &p2lca);
  assert(is_path_valid(p1));
  assert(is_path_valid(p2));
  printf("common_path = %s, restings=%s and %s\n", lca_path, p1lca, p2lca);
  free(lca_path);

  
  p1 = "/x/y/";
  p2 = "/x/";
  lca_path = path_lca_move(p1, p2, &p1lca, &p2lca);
  assert(is_path_valid(p1));
  assert(is_path_valid(p2));
  printf("common_path = %s, restings=%s and %s\n", lca_path, p1lca, p2lca);
  free(lca_path);
}

void dumb_fucking_edgecase()
{
  Tree* tr = tree_new();
  int e1 = tree_move(tr, "/c/", "/c/");
  int e2 = tree_create(tr, "/c/");
  int e3 = tree_move(tr, "/c/", "/c/");
  printf("errs: \"%s\", \"%s\", \"%s\"\n", strerror(e1), strerror(e2), strerror(e3));
  tree_free(tr);
}

int main(void)
{
  simple_tree_test();
  errors_tree_test();
  move_test_async();
  test2();
  test_lca();
  dumb_fucking_edgecase();
  
  return 0;
}
