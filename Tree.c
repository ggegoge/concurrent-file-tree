/**
 * Implementation of the Tree.h interface for allowing various concurrent
 * operations on a direcotry tree like structure.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "err.h"
#include "HashMap.h"
#include "path_utils.h"
#include "rw.h"
#include "Tree.h"

/** This is the root directory name. */
#define ROOT_PATH "/"

/**
 * A macro for centralised function exiting with an error code. It assumes that
 * there is an `int err` declared previously and a label `exiting` at which it
 * should go had the function detected an error. All hail exceptions innit.
 */
#define ERROR(errno)                            \
  do {                                          \
    err = errno;                                \
    goto exiting;                               \
  } while(0)

/**
 * This is a recursive data structure representing a directory tree. It keeps
 * a r&w monitor for access protection.
 */
struct Tree {
  Monitor mon;
  char* dir_name;
  HashMap* subdirs;
};

/**
 * A helper function for creating a heap allocated new empty directory with
 * a given name. Copies the dname string.
 */
static Tree* new_dir(const char* dname)
{
  Tree* tree = malloc(sizeof(Tree));

  if (!tree)
    return NULL;

  tree->dir_name = strdup(dname);

  if (!tree->dir_name) {
    free(tree);
    return NULL;
  }

  tree->subdirs = hmap_new();

  if (!tree->subdirs) {
    free(tree->dir_name);
    free(tree);
    return NULL;
  }

  if (monit_init(&tree->mon)) {
    free(tree->dir_name);
    hmap_free(tree->subdirs);
    free(tree);
    return NULL;
  }

  return tree;
}

/**
 * Find a directory under a `path` and lock it and the path leading to it.
 * Saves the result under `dest`, returns some errno.
 *
 * If an error occured or the directory under `path` does not exist then `*dest`
 * will be set to `NULL`.
 *
 * The `entry_fn` function will be used to access each of the passed by dirs'
 * monitors. It returns an error code and its parameters are the monitor in
 * question and a boolean flag telling it whether it is visiting the target
 * monitor or one on the way.
 *
 * The `passedby` array will be filled with all monitors that were entered in
 * the process (apart from the destination one! It will be in the returned
 * tree).  Its size will be stored under `passed_count`. Exiting them should be
 * done by the caller accordingly with how they've chosen to enter them.
 */
static int access_dir(Tree* root, const char* target, Tree** dest,
                      int entry_fn(Monitor*, bool), Monitor* passedby[],
                      size_t* passed_count)
{
  char component[MAX_DIR_NAME_LEN + 1];
  Tree* next;
  int err;

  *passed_count = 0;
  *dest = root;

  while ((target = split_path(target, component))) {
    if (!*dest)
      break;

    err = entry_fn(&(*dest)->mon, false);

    if (err) {
      *dest = NULL;
      return err;
    }

    passedby[(*passed_count)++] = &(*dest)->mon;
    next = hmap_get((*dest)->subdirs, component);
    *dest = next;
  }

  if (*dest)
    err = entry_fn(&(*dest)->mon, true);

  if (err) {
    *dest = NULL;
    return err;
  }

  return 0;
}

/** Exit monitors according to a given policy. */
static void exit_monitors(Monitor* mons[], size_t count, int exit_fn(Monitor*))
{
  int err;

  for (; count --> 0; ) {
    err = exit_fn(mons[count]);
    /* failure to unlock some parts of the tree breaks it beyond repair */
    syserr(err, "exit_monitors: Failed to exit %lu'th  monitor", count);
  }
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
 * For using the `access_dir` without any protection. This function is a mere
 * no-op satisfying the `entry_fn` signature.
 */
static int chill_entry(Monitor* mon, bool islast)
{
  (void)mon;
  (void)islast;
  return 0;
}

/**
 * Take hold of two directories simultaneously. This bears some resemblance to
 * the classic hungry philosophers problem where philosophers require posessing
 * two forks. In this analogy those are two directories (eg. in `tree_move`).
 * Naïve sequential taking of the two forks one by one won't work as we may
 * starve ourselves with a fellow philosopher. Breaking the symetry is not
 * really possible neither.
 *
 * Solution: join the forks with a string and take the string.
 * Translated to the actual dir tree: lock the LCA of the directories first.
 *
 * This function accesses both the contents under `p1` and `p2` and locks
 * writerly the `lca` dir and readlocks its ancestors (`edit_entry`). It does not
 * however lock `t1` and `t2` (`chill_entry`).
 */
static int double_access(const char* p1, const char* p2, Tree* tree,
                         Tree** lca, Tree** t1, Tree** t2,
                         Monitor* passedby[], size_t* passed_count)
{
  const char* p1lca;
  const char* p2lca;
  Monitor* ignorepassed[MAX_PATH_LEN / 2];
  size_t ignored;
  char* lca_path = path_lca(p1, p2, &p1lca, &p2lca);
  int err = 0;

  if (!lca_path) {
    *passed_count = 0;
    *lca = NULL;
    return ENOMEM;
  }

  err = access_dir(tree, lca_path, lca, edit_entry, passedby, passed_count);
  free(lca_path);

  if (err)
    return err;

  /* Having locked the lca I am free to take the other two without locks */
  err = access_dir(*lca, p1lca, t1, chill_entry, ignorepassed, &ignored);
  assert(!err);
  err = access_dir(*lca, p2lca, t2, chill_entry, ignorepassed, &ignored);
  assert(!err);

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

  monit_destroy(&tree->mon);
  hmap_free(tree->subdirs);
  free(tree->dir_name);
  free(tree);
}

char* tree_list(Tree* tree, const char* path)
{
  Tree* dir;
  char* contents;
  Monitor* passedby[MAX_PATH_LEN / 2];
  size_t passed_count;
  int err;

  if (!is_path_valid(path))
    return NULL;

  err = access_dir(tree, path, &dir, list_entry, passedby, &passed_count);

  if (err || !dir) {
    exit_monitors(passedby, passed_count, reader_exit);
    return NULL;
  }

  contents = make_map_contents_string(dir->subdirs);

  reader_exit(&dir->mon);
  exit_monitors(passedby, passed_count, reader_exit);

  return contents;
}

int tree_create(Tree* tree, const char* path)
{
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

  err = access_dir(tree, parent_path, &parent, edit_entry,
                   passedby, &passed_count);
  free(parent_path);

  if (err)
    ERROR(err);

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
    writer_exit(&parent->mon);

  exit_monitors(passedby, passed_count, reader_exit);
  return err;
}

int tree_remove(Tree* tree, const char* path)
{
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
  err = access_dir(tree, parent_path, &parent, edit_entry,
                   passedby, &passed_count);
  free(parent_path);

  if (err)
    ERROR(err);

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
    writer_exit(&parent->mon);

  exit_monitors(passedby, passed_count, reader_exit);
  return err;
}

/** The critical section of the moving process. */
static int crit_tree_move(Tree* source_parent, Tree* target_parent,
                          const char* source_dir_name,
                          const char* target_dir_name)
{
  Tree* target_dir;
  HashMap* tmp;
  Tree* source_dir = hmap_get(source_parent->subdirs, source_dir_name);

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
  tmp = target_dir->subdirs;
  target_dir->subdirs = source_dir->subdirs;
  source_dir->subdirs = tmp;
  tree_free(source_dir);

  return 0;
}

int tree_move(Tree* tree, const char* source, const char* target)
{
  Tree* lca;
  Tree* source_parent;
  char* source_parent_path;
  char source_name[MAX_DIR_NAME_LEN + 1];
  Tree* target_parent;
  char* target_parent_path;
  char target_name[MAX_DIR_NAME_LEN + 1];
  int err = 0;
  Monitor* passedby[MAX_PATH_LEN / 2];
  size_t passed_count;

  if (!is_path_valid(source) || !is_path_valid(target))
    return EINVAL;
  else if (strcmp(source, ROOT_PATH) == 0)
    return EBUSY;
  /* mv /a/b/ /a/b/c/ is stupid! */
  else if (is_proper_subpath(source, target))
    return ESUBPATH;

  source_parent_path = make_path_to_parent(source, source_name);
  target_parent_path = make_path_to_parent(target, target_name);

  if (!target_parent_path) {
    free(source_parent_path);
    return EEXIST;
  }

  err = double_access(source_parent_path, target_parent_path, tree, &lca,
                      &source_parent, &target_parent, passedby, &passed_count);

  free(source_parent_path);
  free(target_parent_path);

  if (err)
    ERROR(err);

  if (!lca || !source_parent || !target_parent)
    ERROR(ENOENT);

  err = crit_tree_move(source_parent, target_parent, source_name, target_name);

  if (err)
    ERROR(err);

exiting:
  /* only exiting the lca as we didn't lock others (we "chill_entered" them) */
  if (lca)
    writer_exit(&lca->mon);

  exit_monitors(passedby, passed_count, reader_exit);
  return err;
}
