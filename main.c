#include "HashMap.h"
#include "Tree.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

void print_map(HashMap* map)
{
  const char* key = NULL;
  void* value = NULL;
  printf("Size=%zd\n", hmap_size(map));
  HashMapIterator it = hmap_iterator(map);

  while (hmap_next(map, &it, &key, &value)) {
    printf("Key=%s Value=%p\n", key, value);
  }

  printf("\n");
}


void simple_tree_test()
{
  printf("SImple tree test\n");
  Tree* root = tree_new();
  int e1 = tree_create(root, "/a/");
  int e2 = tree_create(root, "/b/");

  if (e1 || e2)
    printf("e1 = %d, e2 = %d\n", e1, e2);

  char* listing = tree_list(root, "/");
  printf("listing: %s\n", listing);
  free(listing);
  int e3 = tree_create(root, "/a/b/");
  int e4 = tree_create(root, "/a/c/");

  listing = tree_list(root, "/a/");
  printf("listing: %s\n", listing);
  free(listing);

  int e5 = tree_move(root, "/a/", "/b/chuuj/");

  if (e3 || e4 || e5)
    printf("e3 = %d, e4 = %d, e5 = %d\n", e3, e4, e5);

  listing = tree_list(root, "/");
  printf("listing: %s\n", listing);
  free(listing);

  listing = tree_list(root, "/a/");
  printf("listing: %s\n", listing);
  free(listing);

  listing = tree_list(root, "/b/");
  printf("listing: %s\n", listing);
  free(listing);

  listing = tree_list(root, "/b/chuuj/");
  printf("listing: %s\n", listing);
  free(listing);

  tree_free(root);
}

int main(void)
{
  HashMap* map = hmap_new();
  hmap_insert(map, "a", hmap_new());
  print_map(map);

  HashMap* child = (HashMap*)hmap_get(map, "a");
  hmap_free(child);
  hmap_remove(map, "a");
  print_map(map);

  hmap_free(map);

  simple_tree_test();

  return 0;
}
