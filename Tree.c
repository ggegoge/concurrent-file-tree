/**
 * Implementation of the Tree.h interface for allowing (TODO concurrent) various
 * operations on a direcotry tree like structure.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "HashMap.h"
#include "path_utils.h"
#include "Tree.h"

/* TODO: the three fnctions
 *   1) tree_create
 *   2) tree_remove
 *   3) tree_move
 * have some striking similairities (they all access the parent and then do
 * something with it). Can we somehow refactor that? I'm sure we can...
 * But returning errnos in various places makes it wearisome af.
 * also i cannot refactor a removing function even though i need it in both
 * move and remove but in the snd one i do it only on a non-empty dir... */

/* find parent and a dir? this sort of refactor? hard asf as i need to remember
 * the subdir name after this function would get called in most of the cases
 * jfc */
/* void find_parent_dir(Tree* tree, const char* path, Tree** parent, Tree** subdir); */


/** This is the root directory name. */
#define ROOT_PATH "/"

/** This is a recursive data structure representing a directory tree. */
struct Tree {
  char* dir_name;
  HashMap* subdirs;
};

/**
 * A helper function for creating a heap allocated new empty directory with
 * a given name. Copies the dname string.
 */
Tree* new_dir(const char* dname)
{
  Tree* tree = malloc(sizeof(Tree));
  char* name = strdup(dname);

  if (!tree || !dname)
    return NULL;

  tree->dir_name = name;
  tree->subdirs = hmap_new();

  return tree;
}

/**
 * Find the tree corresponding to a given `path`. Simple dir tree search.
 * Returns `NULL` if no such directory exists.
 */
Tree* find_dir(Tree* root, const char* path)
{
  char component[MAX_FOLDER_NAME_LENGTH + 1];
  const char* subpath = path;

  while ((subpath = split_path(subpath, component))) {
    if (!root)
      break;

    root = hmap_get(root->subdirs, component);
  }

  return root;
}

Tree* tree_new()
{
  return new_dir(ROOT_PATH);
}

/** Free all the memory stored by a tree in a recursive manner. */
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

char* tree_list(Tree* tree, const char* path)
{
  printf("ls %s\n", path);
  Tree* dir;

  if (!is_path_valid(path))
    return 0;

  dir = find_dir(tree, path);

  if (!dir)
    return 0;

  return make_map_contents_string(dir->subdirs);
}

int tree_create(Tree* tree, const char* path)
{
  printf("mdkir %s\n", path);
  Tree* parent;
  Tree* subdir;
  char* parent_path;
  char last_component[MAX_FOLDER_NAME_LENGTH + 1];

  if (!is_path_valid(path))
    return EINVAL;

  parent_path = make_path_to_parent(path, last_component);

  /* Create called on "/" -- the root already exists. */
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

  if (!subdir)
    return ENOMEM;

  /* Add the newly created subdirectory as a parent's child */
  hmap_insert(parent->subdirs, subdir->dir_name, subdir);
  return 0;
}

int tree_remove(Tree* tree, const char* path)
{
  printf("rmdir %s\n", path);
  Tree* parent;
  Tree* subdir;
  char* parent_path;
  char last_component[MAX_FOLDER_NAME_LENGTH + 1];

  if (!is_path_valid(path))
    return EINVAL;
  else if (strcmp(path, ROOT_PATH) == 0)
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

int tree_move(Tree* tree, const char* source, const char* target)
{
  printf("mv %s %s\n", source, target);
  Tree* source_parent;
  char* source_parent_path;
  Tree* source_dir;
  char source_dir_name[MAX_FOLDER_NAME_LENGTH + 1];
  Tree* target_parent;
  char* target_parent_path;
  Tree* target_dir;
  char target_dir_name[MAX_FOLDER_NAME_LENGTH + 1];

  if (!is_path_valid(source) || !is_path_valid(target))
    return EINVAL;
  else if (strcmp(source, ROOT_PATH) == 0)
    return EBUSY;
  else if (is_subpath(source, target))
    return ESUBPATH;
  
  source_parent_path = make_path_to_parent(source, source_dir_name);
  target_parent_path = make_path_to_parent(target, target_dir_name);
  source_parent = find_dir(tree, source_parent_path);
  target_parent = find_dir(tree, target_parent_path);

  free(source_parent_path);
  free(target_parent_path);

  if (!source_parent || !target_parent)
    return ENOENT;

  source_dir = hmap_get(source_parent->subdirs, source_dir_name);

  if (!source_dir)
    return ENOENT;

  if (hmap_get(target_parent->subdirs, target_dir_name))
    return EEXIST;

  /* remove ourselves from one map and add to another */
  hmap_remove(source_parent->subdirs, source_dir_name);
  target_dir = new_dir(target_dir_name);

  if (!target_dir)
    return ENOMEM;

  hmap_insert(target_parent->subdirs, target_dir->dir_name, target_dir);

  /* move the contents now and get rid of the old dir */
  HashMap* tmp = target_dir->subdirs;
  target_dir->subdirs = source_dir->subdirs;
  source_dir->subdirs = tmp;
  tree_free(source_dir);

  return 0;
}
