/**
 * Implementation of the Tree.h interface for allowing (TODO concurrent) various
 * operations on a direcotry tree like structure.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#include <assert.h>
#include <stdio.h>

#include "HashMap.h"
#include "path_utils.h"
#include "rw.h"
#include "Tree.h"

/** This is the root directory name. */
#define ROOT_PATH "/"

/**
 * A macro for centralised function exiting with an error code. It assumes that
 * there is an `int err` declared previously and a label `exiting` at which it
 * should go had the function detected an error.
 */
#define ERROR(errno)                            \
  do {                                          \
    err = errno;                                \
    goto exiting;                               \
  } while(0)

/**
 * This is a recursive data structure representing a directory tree. It keeps
 * a monitor for access protection.
 */
struct Tree {
  Monitor monit;
  char* dir_name;
  HashMap* subdirs;
};

pthread_mutex_t malloc_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * A helper function for creating a heap allocated new empty directory with
 * a given name. Copies the dname string.
 */
static Tree* new_dir(const char* dname)
{
  int e = 0;
  e = pthread_mutex_lock(&malloc_mutex);

  if (e)
    return NULL;

  Tree* tree = malloc(sizeof(Tree));

  if (!tree) {
    e = pthread_mutex_unlock(&malloc_mutex);
    return NULL;
  }

  tree->dir_name = strdup(dname);

  if (!tree->dir_name) {
    free(tree);
    e = pthread_mutex_unlock(&malloc_mutex);
    return NULL;
  }

  tree->subdirs = hmap_new();

  if (!tree->subdirs) {
    free(tree->dir_name);
    free(tree);
    e = pthread_mutex_unlock(&malloc_mutex);
    return NULL;
  }

  if (monit_init(&tree->monit)) {
    free(tree->dir_name);
    hmap_free(tree->subdirs);
    free(tree);
    e = pthread_mutex_unlock(&malloc_mutex);
    return NULL;
  }

  e = pthread_mutex_unlock(&malloc_mutex);
  return tree;
}

/**
 * Find a directory under a `path` and lock it.
 * The `entry_fn` function will be used to access each of the passed by dirs'
 * monitors. It returns an exit code and its parameters are the monitor in
 * question and a boolean flag telling whether it is visiting the target monitor
 * or one on the way.
 *
 * The `passedby` array will be filled with all of monitors that were entered
 * in the process (apart from the last one!).
 * Its size will be stored under `passed_count`. Exiting them should be done by
 * the caller accordingly with how they've chosen to enter them.
 */
static Tree* access_dir(Tree* root, const char* path,
                        int entry_fn(Monitor*, bool),
                        Monitor* passedby[], size_t* passed_count)
{
  char component[MAX_DIR_NAME_LEN + 1];
  const char* subpath = path;
  Tree* next;

  *passed_count = 0;

  while ((subpath = split_path(subpath, component))) {
    if (!root)
      break;

    printf("\tacess[%lu]: non final entry on %s\n", pthread_self(), root->dir_name);
    entry_fn(&root->monit, false);
    printf("\t%lu: passedby[%lu] = %s\n", pthread_self(), *passed_count,
           root->dir_name);
    passedby[(*passed_count)++] = &root->monit;
    next = hmap_get(root->subdirs, component);
    root = next;
  }

  if (root) {
    printf("\tacess[%lu]: final entry on %s\n", pthread_self(), root->dir_name);
    entry_fn(&root->monit, true);
  }

  return root;
}

/** Exit monitors according to a given policy. */
static int exit_monitors(Monitor* mons[], size_t count, int exit_fn(Monitor*))
{
  printf("%lu: EXIT %lu MONITORS\n", pthread_self(), count);

  for (; count -- > 0; ) {
    printf("%lu exiting pasedby[%lu]\n", pthread_self(), count);
    exit_fn(mons[count]);
  }

  return 0;
}

/**
 * This is how the editing operations will enter directories. Depending on
 * whether this is their destination dir (`islast`) they will either lock it as
 * a writer or as a reader. Meaning that we treat an editing operation on an
 * ancestoral directory as a reader and it's a writer only on the last one.
 *
 * This function has a signature appropriate for the `entry_fn` argument in
 * `access_dir`.
 */
static int edit_entry(Monitor* mon, bool islast)
{
  if (islast)
    return writer_entry(mon);
  else
    return reader_entry(mon);
}

/**
 * For `tree_list` accessing of directories. A wrapper for reader's dir access.
 * Matches `entry_fn` in `access_dir`.
 */
static int list_entry(Monitor* mon, bool islast)
{
  (void)islast;
  return reader_entry(mon);
}

/**
 * For using the `access_dir` without any protection. This function is a no-op
 * satisfying the `entry_fn` signature.
 */
static int chill_entry(Monitor* mon, bool islast)
{
  (void)mon;
  (void)(islast);
  return 0;
}

/* -------------------------------------------------------------------------- */

Tree* tree_new()
{
  return new_dir(ROOT_PATH);
}

void tree_free(Tree* tree)
{
  HashMapIterator it = hmap_iterator(tree->subdirs);
  const char* subdir_name;
  void* subdir_ptr;
  Tree* subdir;

  /* freeing descendants first recursively */
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
  printf("ls %s     | %lu\n", path, pthread_self());
  Tree* dir;
  char* contents;
  Monitor* passedby[MAX_PATH_LEN / 2];
  size_t passed_count;

  if (!is_path_valid(path))
    return NULL;

  dir = access_dir(tree, path, list_entry, passedby, &passed_count);

  if (!dir) {
    exit_monitors(passedby, passed_count, reader_exit);
    return NULL;
  }

  contents = make_map_contents_string(dir->subdirs);
  printf("\tlist: reader exiting\n");

  reader_exit(&dir->monit);
  exit_monitors(passedby, passed_count, reader_exit);

  return contents;
}

int tree_create(Tree* tree, const char* path)
{
  printf("mdkir %s     | %lu\n", path, pthread_self());
  Tree* parent;
  Tree* subdir;
  char* parent_path;
  char last_component[MAX_DIR_NAME_LEN + 1];
  int err = 0;
  Monitor* passedby[MAX_PATH_LEN / 2];
  size_t passed_count;

  if (!is_path_valid(path))
    return EINVAL;

  parent_path = make_path_to_parent(path, last_component);

  /* Create called on "/" -- the root already exists. */
  if (!parent_path)
    return EEXIST;

  parent = access_dir(tree, parent_path, edit_entry, passedby, &passed_count);
  free(parent_path);

  /* The parent does not exist. */
  if (!parent)
    ERROR(ENOENT);

  /* The subdir we want to create already exists. */
  if (hmap_get(parent->subdirs, last_component))
    ERROR(EEXIST);

  subdir = new_dir(last_component);

  if (!subdir)
    ERROR(ENOMEM);

  /* Add the newly created subdirectory as a parent's child */
  hmap_insert(parent->subdirs, subdir->dir_name, subdir);

exiting:

  if (parent)
    writer_exit(&parent->monit);

  exit_monitors(passedby, passed_count, reader_exit);
  return err;
}

int tree_remove(Tree* tree, const char* path)
{
  printf("rmdir %s     | %lu\n", path, pthread_self());
  Tree* parent;
  Tree* subdir;
  char* parent_path;
  char last_component[MAX_DIR_NAME_LEN + 1];
  int err = 0;
  Monitor* passedby[MAX_PATH_LEN / 2];
  size_t passed_count;

  if (!is_path_valid(path))
    return EINVAL;
  else if (strcmp(path, ROOT_PATH) == 0)
    return EBUSY;

  parent_path = make_path_to_parent(path, last_component);
  parent = access_dir(tree, parent_path, edit_entry, passedby, &passed_count);
  free(parent_path);

  if (!parent)
    ERROR(ENOENT);

  subdir = hmap_get(parent->subdirs, last_component);

  if (!subdir)
    ERROR(ENOENT);

  if (hmap_size(subdir->subdirs) > 0)
    ERROR(ENOTEMPTY);

  hmap_remove(parent->subdirs, last_component);
  tree_free(subdir);

exiting:

  if (parent)
    writer_exit(&parent->monit);

  exit_monitors(passedby, passed_count, reader_exit);
  return err;
}

/**
 * If we think about it then this bears some resemblence to the classic hungry
 * philosophers problem. Each `tree_move` recquires posessing two ``forks''. In
 * this analogy those are source's parent dir and target's parent dir. Naïve
 * sequential taking of the two forks one by one won't work as we may starve
 * ourselves with a fellow philosopher. Breaking the symetry is not really
 * possible neither.
 *
 * Solution: ``join the forks with a string and take the string.''
 * Translated to the actual tree: lock the LCA of target and source first.
 *
 * This function accesses both the contents under p1 and p2 and locks quite
 * writerly the lca dir and readlocks its ancestors. As in other functions the
 * array `passedby` of size `passed_count` will store those locked on the way.
 */
static void double_access(const char* p1, const char* p2, Tree* tree,
                          Tree** lca, Tree** t1, Tree** t2,
                          Monitor* passedby[], size_t* passed_count)
{
  const char* p1lca;
  const char* p2lca;
  Monitor* ignorepassed[MAX_PATH_LEN / 2];
  size_t ignored;

  /* char* lca_path = path_lca(p1, p2); */
  char* lca_path = path_lca_move(p1, p2, &p1lca, &p2lca);
  printf("common_path = %s, restings=%s and %s\n", lca_path, p1lca, p2lca);

  *lca = access_dir(tree, lca_path, edit_entry, passedby, passed_count);
  printf("%lu: got the lca ie %s!\n", pthread_self(), lca_path);
  /* Having locked the lca I am free to take the other two. */
  printf("%lu tring to acquire source par ie %s\n", pthread_self(), p1);
  *t1 = access_dir(*lca, p1lca, chill_entry, ignorepassed, &ignored);
  printf("%lu: got the source parent ie %s!\n", pthread_self(), p1);
  printf("%lu tring to acquire targt par ie %s\n", pthread_self(), p2);
  *t2 = access_dir(*lca, p2lca, chill_entry, ignorepassed, &ignored);
  printf("%lu got the target parent ie %s!\n", pthread_self(), p2);

  free(lca_path);
}

int tree_move(Tree* tree, const char* source, const char* target)
{
  printf("mv %s %s    | %lu\n", source, target, pthread_self());
  Tree* lca;
  Tree* source_parent;
  char* source_parent_path;
  Tree* source_dir;
  char source_dir_name[MAX_DIR_NAME_LEN + 1];
  Tree* target_parent;
  char* target_parent_path;
  Tree* target_dir;
  char target_dir_name[MAX_DIR_NAME_LEN + 1];
  int err = 0;
  HashMap* tmp;
  Monitor* passedby[MAX_PATH_LEN / 2];
  size_t passed_count;

  if (!is_path_valid(source) || !is_path_valid(target))
    return EINVAL;
  else if (strcmp(source, ROOT_PATH) == 0)
    return EBUSY;
  /* mv /a/b/ /a/b/c/ is stupid! */
  else if (is_subpath(source, target))
    return ESUBPATH;

  source_parent_path = make_path_to_parent(source, source_dir_name);
  target_parent_path = make_path_to_parent(target, target_dir_name);

  if (!target_parent_path) {
    free(source_parent_path);
    return EEXIST;
  }

  double_access(source_parent_path, target_parent_path, tree,
                &lca, &source_parent, &target_parent, passedby, &passed_count);

  free(source_parent_path);
  free(target_parent_path);

  if (!lca || !source_parent || !target_parent)
    ERROR(ENOENT);

  source_dir = hmap_get(source_parent->subdirs, source_dir_name);

  if (!source_dir)
    ERROR(ENOENT);

  if (hmap_get(target_parent->subdirs, target_dir_name))
    ERROR(EEXIST);

  /* remove ourselves from one map and add to another */
  hmap_remove(source_parent->subdirs, source_dir_name);
  target_dir = new_dir(target_dir_name);

  if (!target_dir)
    ERROR(ENOMEM);

  hmap_insert(target_parent->subdirs, target_dir->dir_name, target_dir);

  /* move the contents now and get rid of the old dir */
  tmp = target_dir->subdirs;
  target_dir->subdirs = source_dir->subdirs;
  source_dir->subdirs = tmp;
  tree_free(source_dir);

exiting:

  /* only exiting the lca as we didn't lock others (we chill_entered them) */
  if (lca)
    writer_exit(&lca->monit);

  exit_monitors(passedby, passed_count, reader_exit);
  return err;
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
