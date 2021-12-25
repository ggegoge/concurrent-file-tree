
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "HashMap.h"
#include "Tree.h"

struct Tree {
  char* dir_name;
  HashMap* subdirs;
};

/**
 * A helper function for creating a new empty directory with a given name.
 * It takes ownership of memory allocated for the name string.
 */
Tree* new_dir(char* name)
{
  Tree* tree = malloc(sizeof(Tree));

  if (!tree)
    exit(1);
  
  tree->dir_name = name;
  tree->subdirs = hmap_new();

  return tree;
}

Tree* tree_new()
{
  char* name = strdup("/");
  return new_dir(name);
}

/* Free all the memory stored by a tree. Does it in a recursive manner. */
void tree_free(Tree* tree)
{
  HashMapIterator it = hmap_iterator(tree->subdirs);
  const char* subdir_name;
  void* subdir_ptr;

  while (hmap_next(tree->subdirs, &it, &subdir_name, &subdir_ptr)) {
    Tree* subdir = (Tree*)subdir_ptr;
    tree_free(subdir);
  }

  hmap_free(tree->subdirs);
  free(tree->dir_name);
  free(tree);
}

