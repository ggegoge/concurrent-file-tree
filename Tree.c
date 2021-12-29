/**
 * Implementation of the Tree.h interface for allowing (TODO concurrent) various
 * operations on a direcotry tree like structure.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <assert.h>
#include <unistd.h>

#include "HashMap.h"
#include "path_utils.h"
#include "rw.h"
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

/**
 * This is a recursive data structure representing a directory tree.
 * One mutex and two conditionals per node. We will do this the following way:
 * readers & writers but with listers & anything-else'ers.
 *
 * Also: if there are changes somewhere then you shouldn't proceed to visit its
 * descendants until those take place.
 */
struct Tree {
  Monitor monit;
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

  /* TODO: when fails we shoyld free the allocated ones
   * so more ifology after each malloc */
  if (!tree || !name)
    return NULL;

  tree->dir_name = name;
  tree->subdirs = hmap_new();

  if (monit_init(&tree->monit))
    return NULL;

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

/**
 * Find a directory under a `path`. The `entry_fn` function will be used to
 * access each of the passed by `Tree`'s monitor. It returns an exit code and
 * its parameters are the monitor in question and a boolean flag telling whether
 * it is visiting the target's node monitor or one on the way.
 *
 * The `exit_fn` function works similarily but it will only be called on non-final
 * directories. */
Tree* access_dir(Tree* root, const char* path, int entry_fn(Monitor*, bool),
                 int exit_fn(Monitor*))
{
  char component[MAX_FOLDER_NAME_LENGTH + 1];
  const char* subpath = path;
  Tree* next;

  while ((subpath = split_path(subpath, component))) {
    if (!root)
      break;

    printf("\tacess[%lu]: non final entry on %s\n", pthread_self(), root->dir_name);
    entry_fn(&root->monit, false);
    next = hmap_get(root->subdirs, component);
    printf("\tacess[%lu]: exit on %s\n", pthread_self(), root->dir_name);
    exit_fn(&root->monit);
    root = next;
  }

  if (root) {
    printf("\tacess[%lu]: final entry on %s\n", pthread_self(), root->dir_name);
    entry_fn(&root->monit, true);
  }

  return root;
}

/**
 * This is how the editing thread will enter directories. Depending on whether
 * this is their destination dir (`islast`) they will either be treated as a
 * writer or as a reader. Meaning that we treat an editing operation on an
 * ancestoral directory as a reader and it's a writer only on the last one.
 *
 * Note: this function has a signature appropriate for the `entry_fn` argument
 * in `access_dir`.
 */
int edit_entry(Monitor* mon, bool islast)
{
  if (islast)
    return writer_entry(mon);
  else
    return reader_entry(mon);
}

/** A wrapper for reader's dir access. Matches `entry_fn` in `access_dir`. */
int list_entry(Monitor* mon, bool islast)
{
  (void)islast;
  return reader_entry(mon);
}

/* -------------------------------------------------------------------------- */

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
  Tree* subdir;

  while (hmap_next(tree->subdirs, &it, &subdir_name, &subdir_ptr)) {
    subdir = (Tree*)subdir_ptr;
    tree_free(subdir);
  }

  monit_destroy(&tree->monit);
  hmap_free(tree->subdirs);
  free(tree->dir_name);
  free(tree);
}

char* tree_list(Tree* tree, const char* path)
{
  printf("ls %s\n", path);
  Tree* dir;
  char* contents;

  if (!is_path_valid(path))
    return 0;

  dir = access_dir(tree, path, list_entry, reader_exit);

  if (!dir)
    return 0;

  contents = make_map_contents_string(dir->subdirs);
  printf("\tlist: reader exiting\n");
  reader_exit(&dir->monit);

  return contents;
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

  parent = access_dir(tree, parent_path, edit_entry, reader_exit);
  free(parent_path);

  /* The parent does not exist. */
  if (!parent)
    return ENOENT;

  /* The subdir we want to create already exists. */
  if (hmap_get(parent->subdirs, last_component)) {
    writer_exit(&parent->monit);
    return EEXIST;
  }

  subdir = new_dir(last_component);

  if (!subdir) {
    writer_exit(&parent->monit);
    return ENOMEM;
  }

  /* Add the newly created subdirectory as a parent's child */
  hmap_insert(parent->subdirs, subdir->dir_name, subdir);
  writer_exit(&parent->monit);

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
  parent = access_dir(tree, parent_path, edit_entry, reader_exit);
  free(parent_path);

  if (!parent) {
    writer_exit(&parent->monit);
    return ENOENT;
  }
  
  subdir = hmap_get(parent->subdirs, last_component);

  if (!subdir) {
    writer_exit(&parent->monit);
    return ENOENT;
  }

  if (hmap_size(subdir->subdirs) > 0) {
    writer_exit(&parent->monit);
    return ENOTEMPTY;
  }

  hmap_remove(parent->subdirs, last_component);
  tree_free(subdir);
  writer_exit(&parent->monit);

  return 0;
}

/* If we think about it then this bears some resemblence to the classic hungry
 * philosophers problem. Each `tree_move` recquires posessing two ``forks'' in
 * this analogy those are source's parent dir and target's parent dir. Naïve
 * sequential taking of the forks won't work as we may starve ourselves with
 * a fellow philosopher. Breaking the symetry is not really possible neither.
 *
 * Solution: join the forks with a string and take the string.
 * Translated to the actual tree: lock the LCA of target and source
 * and only then proceed. */
int tree_move(Tree* tree, const char* source, const char* target)
{
  printf("mv %s %s    | %lu\n", source, target, pthread_self());
  char* lca_path;
  Tree* lca;
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

  lca_path = path_lca(source, target);
  printf("lca_path = %s\n", lca_path);
  source_parent_path = make_path_to_parent(source, source_dir_name);
  target_parent_path = make_path_to_parent(target, target_dir_name);
  if (!target_parent_path) {
    free(source_parent_path);
    return EEXIST;
  }

  lca = access_dir(tree, lca_path, edit_entry, reader_exit);
  /* having locked the lca i may proceed from there onwards may i not? */
  source_parent = access_dir(tree, source_parent_path, edit_entry, reader_exit);
  printf("%lu: got the source parent!\n", pthread_self());
  target_parent = access_dir(tree, target_parent_path, edit_entry, reader_exit);
  printf("%lu got the target parent!\n", pthread_self());

  free(lca_path);
  free(source_parent_path);
  free(target_parent_path);

  if (!lca || !source_parent || !target_parent) {
    printf("nie ma chuja\n");
    writer_exit(&source_parent->monit);
    writer_exit(&target_parent->monit);
    writer_exit(&lca->monit);
    return ENOENT;
  }

  source_dir = hmap_get(source_parent->subdirs, source_dir_name);

  if (!source_dir) {
    printf("nie istnieje gosc z nazwa %s\n", source_dir_name);
    writer_exit(&source_parent->monit);
    writer_exit(&target_parent->monit);
    writer_exit(&lca->monit);
    return ENOENT;
  }

  if (hmap_get(target_parent->subdirs, target_dir_name)) {
    printf("nie istnieje gosc z nazwa %s\n", target_dir_name);
    writer_exit(&source_parent->monit);
    writer_exit(&target_parent->monit);
    writer_exit(&lca->monit);
    return EEXIST;
  }

  /* remove ourselves from one map and add to another */
  hmap_remove(source_parent->subdirs, source_dir_name);
  target_dir = new_dir(target_dir_name);

  if (!target_dir) {
    writer_exit(&source_parent->monit);
    writer_exit(&target_parent->monit);
    writer_exit(&lca->monit);
    return ENOMEM;
  }

  hmap_insert(target_parent->subdirs, target_dir->dir_name, target_dir);

  /* move the contents now and get rid of the old dir */
  HashMap* tmp = target_dir->subdirs;
  target_dir->subdirs = source_dir->subdirs;
  source_dir->subdirs = tmp;
  tree_free(source_dir);

  writer_exit(&source_parent->monit);
  writer_exit(&target_parent->monit);
  writer_exit(&lca->monit);

  return 0;
}

/* list all contents recursively */
void tree_tree(Tree* tree, int depth)
{
  HashMapIterator it = hmap_iterator(tree->subdirs);
  const char* subdir_name;
  void* subdir_ptr;
  Tree* subdir;

  if (!depth)
    printf("tree %s\n", tree->dir_name);

  printf(" ");

  for (int i = 0; i < depth; ++i)
    printf(" ");

  printf("%s\n", tree->dir_name);

  while (hmap_next(tree->subdirs, &it, &subdir_name, &subdir_ptr)) {
    subdir = (Tree*)subdir_ptr;
    tree_tree(subdir, depth + 1);
  }
}
