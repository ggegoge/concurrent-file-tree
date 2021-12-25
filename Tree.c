
/*
 * Implementation of the Tree.h interface for allowing (TODO concurrent) various
 * operations on a direcotry tree like structure.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "HashMap.h"
#include "path_utils.h"
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

/* Find the tree corresponding to a given path. */
Tree* find_dir(Tree* root, const char* path)
{
  char component[MAX_FOLDER_NAME_LENGTH + 1];
  const char* subpath = path;
  assert(is_path_valid(path));
  while ((subpath = split_path(subpath, component))) {
    if (!root)
      break;

    root = hmap_get(root->subdirs, component);
  }

  return root;
}

/* Find a directory and its parent.
 * return val: TODO! what do we want it to return? i want to allow *dir == NULL
 * but i want *parent->dir_name to be precisely equal to the penultimate comp! */
bool find_parent_dir(Tree* root, Tree** parent, Tree** dir, const char* path)
{
  char component[MAX_FOLDER_NAME_LENGTH + 1];
  const char* subpath = path;
  assert(is_path_valid(path));
  *parent = NULL;
  *dir = root;
  while ((subpath = split_path(subpath, component))) {
    /* this is not correct */
    if (!root)
      return false;

    root = hmap_get(root->subdirs, component);
    *parent = *dir;
    *dir = root;
  }

  return true;
}


char* tree_list(Tree* tree, const char* path)
{
  Tree* dir = find_dir(tree, path);

  if (!dir)
    return 0;

  return make_map_contents_string(dir->subdirs);
}

/* TODO -- i want to find the parent directory, then create a new dir with its
 * name being the last component from the path and add it to the hashmap.
 * how to devise the function for finding the parent then? split path but
 * keeping the last component... huh. */
int tree_create(Tree* tree, const char* path)
{  
  Tree* parent;
  Tree* dir;

  /* The parent to the new directory does not exist. */
  if (!find_parent_dir(tree, &parent, &dir, path))
    return ENOENT;

  /* The new directory already exists. */
  if (dir)
    return EEXIST;

  
  return 1;
}
