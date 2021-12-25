
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

#define ROOT_PATH "/"

struct Tree {
  char* dir_name;
  HashMap* subdirs;
};

/**
 * A helper function for creating a new empty directory with a given name.
 * Copies the dname string.
 */
Tree* new_dir(const char* dname)
{
  Tree* tree = malloc(sizeof(Tree));
  char* name = strdup(dname);
  
  if (!tree || !dname)
    exit(1);

  tree->dir_name = name;
  tree->subdirs = hmap_new();

  return tree;
}

Tree* tree_new()
{
  return new_dir(ROOT_PATH);
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

/**
 * Find a directory specified by a path. The directory will be saved under `dir`
 * and its parent will be saved under `parent`. In case the `dir` does not exist
 * then it will be set to `NULL`. In case even the parent directory does not
 * exist then it will be set to `NULL` as well.
 */
void find_parent_dir(Tree* root, Tree** parent, Tree** dir, const char* path)
{
  char component[MAX_FOLDER_NAME_LENGTH + 1];
  const char* subpath = path;
  *parent = NULL;
  *dir = root;

  while ((subpath = split_path(subpath, component))) {
    /* the path has not yet ended yet we have run out of directories to visit */
    if (!root) {
      *parent = *dir;
      return;
    }

    root = hmap_get(root->subdirs, component);
    *parent = *dir;
    *dir = root;
  }
}

char* tree_list(Tree* tree, const char* path)
{
  Tree* dir = find_dir(tree, path);

  if (!dir)
    return 0;

  return make_map_contents_string(dir->subdirs);
}

int tree_create(Tree* tree, const char* path)
{
  Tree* parent;
  Tree* subdir;
  char* parent_path;
  char last_component[MAX_FOLDER_NAME_LENGTH + 1];

  if (!is_path_valid(path))
    return EINVAL;

  parent_path = make_path_to_parent(path, last_component);

  /* Create called on "/" -- the root already exists obviously */
  if (!parent_path)
    return EEXIST;

  parent = find_dir(tree, parent_path);
  free(parent_path);

  /* The parent does not exist. */
  if (!parent)
    return ENOENT;

  /* The subdir we want to create already exists. */
  if (hmap_get(parent->subdirs, last_component))
    return EEXIST;

  subdir = new_dir(last_component);

  /* add the newly created subdirectory as a parent's child */
  hmap_insert(parent->subdirs, subdir->dir_name, subdir);
  return 0;
}
  return 0;
}

/* we find the parent, inside we find the child, check if it is empty, remove
 * the dir from the map and then from the memory. */
int tree_remove(Tree* tree, const char* path)
{
  Tree* parent;
  Tree* dir;
  Tree* subdir;
  char* parent_path;
  char last_component[MAX_FOLDER_NAME_LENGTH + 1];
  
  if (!is_path_valid(path))
    return EINVAL;
  else if (strcmp(path, ROOT_PATH))
    return EBUSY;

  parent_path = make_path_to_parent(path, last_component);
  parent = find_dir(tree, parent_path);
  free(parent_path);

  subdir = hmap_get(parent->subdirs, last_component);
  if (!subdir)
    return ENOENT;

  if (hmap_size(subdir->subdirs) > 0)
    return ENOTEMPTY;

  hmap_remove(parent->subdirs, last_component);
  tree_free(subdir);
  
  return 0;
}

int tree_move(Tree* tree, const char* source, const char* target);
