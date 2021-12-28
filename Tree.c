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

/** Using this structure to store all of a tree's synchronisation variables. */
typedef struct Monitor {
  /* TODO: move this struct as well as the functions that operate on it into
   * a separate rw module. Then store the Monitor on the heap? or perhaps no */
  pthread_mutex_t mutex;
  pthread_cond_t readers;
  pthread_cond_t writers;
  size_t rwait;
  size_t wwait;
  size_t rcount;
  size_t wcount;
  /* id of the current writer! */
  pthread_t wid;
} Monitor;

/**
 * This is a recursive data structure representing a directory tree.
 * One mutex and two conditionals per node. We will do this the following way:
 * readers & writers but with listers & anything-else'ers.
 *
 * Also: if there are changes somewhere then you shouldn't proceed to visit its
 * descendants until those take place.
 */
struct Tree {
  struct Monitor monit;
  char* dir_name;
  HashMap* subdirs;
};

int monit_init(Monitor* mon)
{
  int err;
   /* consciously using "||" operator's laziness */
  if ((err = pthread_mutex_init(&mon->mutex, 0)) ||
      (err = pthread_cond_init(&mon->readers, 0)) ||
      (err = pthread_cond_init(&mon->writers, 0)))
    return err;
  
  mon->rwait = mon->wwait = mon->wcount = mon->rcount = mon->wid = 0;
  return 0;
}

int monit_destroy(Monitor* mon)
{
  int err;
  if ((err = pthread_mutex_destroy(&mon->mutex)) ||
      (err = pthread_cond_destroy(&mon->readers)) ||
      (err = pthread_cond_destroy(&mon->writers)))
    return err;

  mon->rwait = mon->wwait = mon->wcount = mon->rcount = 0;
  return 0;  
}

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
Tree* access_dir(Tree* root, const char* path, int (*entry_fn) (Monitor*, bool),
                 int (*exit_fn) (Monitor*))
{
  char component[MAX_FOLDER_NAME_LENGTH + 1];
  const char* subpath = path;
  Tree* next;

  while ((subpath = split_path(subpath, component))) {
    if (!root)
      break;

    printf("\tacess: non final entry on %s\n", root->dir_name);
    entry_fn(&root->monit, false);
    next = hmap_get(root->subdirs, component);
    printf("\tacess: exit on %s\n", root->dir_name);
    exit_fn(&root->monit);
    root = next;
  }

  if (root) {
    printf("\tacess: final entry on %s\n", root->dir_name);
    entry_fn(&root->monit, true);
  }
  
  return root;  
}

/* Entry and exit protocoles in both reading and writing flavours (reminder: we
 * treat a writer on an ancestoral node to its target as a reader!).
 *
 * Then those will be used neatly for the directory accessing function. */

int writer_entry(Monitor* mon)
{
  int err;

  if (!mon)
    return 0;
  
  err = pthread_mutex_lock(&mon->mutex);

  /* If I'm a writer here then I can write along as I am a sequential being
   * apart from that i wait if there are some others working */
  if (!(mon->wcount > 0 && mon->wid == pthread_self()) &&
      (mon->rwait > 0 || mon->rcount > 0 || mon->wcount > 0)) {
    printf("writer %lu goes to sleep cause rw=%lu rc=%lu wc=%lu\n",
           pthread_self(), mon->rwait, mon->rcount, mon->wcount);
    ++mon->wwait;
    err = pthread_cond_wait(&mon->writers, &mon->mutex);
    printf("writer %lu woke up\n", pthread_self());
    --mon->wwait;
  }

  ++mon->wcount;
  mon->wid = pthread_self();
  printf("\twentr %lu: ++wcount\n", pthread_self());
  assert(mon->wcount > 0 || mon->wid == pthread_self());
  assert(mon->rcount == 0);
  err = pthread_mutex_unlock(&mon->mutex);

  return err;
}

int writer_exit(Monitor* mon)
{
  int err;

  if (!mon)
    return 0;
  
  err = pthread_mutex_lock(&mon->mutex);
  assert(mon->wcount > 0 || mon->wid == pthread_self());
  assert(mon->rcount == 0);    
  --mon->wcount;

  if (mon->wcount == 0)
    mon->wid = 0;
  
  printf("\twexit %lu: --wcount\n", pthread_self());
  
  if (mon->rwait > 0)
    err = pthread_cond_broadcast(&mon->readers);
  else if (mon->wwait > 0)
    err = pthread_cond_signal(&mon->writers);

  err = pthread_mutex_unlock(&mon->mutex);

  return err;
}


int reader_entry(Monitor* mon)
{
  int err;

  if (!mon)
    return 0;

  err = pthread_mutex_lock(&mon->mutex);

  /* I wait if either im not the current owner of this or if i have other more
   * classical reasons like waiting for others to finish their business */
  if (!(mon->wcount > 0 && mon->wid == pthread_self()) &&
      (mon->wwait > 0 || mon->wcount > 0)) {
    printf("reader %lu goes to sleep cause ww=%lu wc=%lu\n",
           pthread_self(), mon->wwait, mon->wcount);
    ++mon->rwait;
    err = pthread_cond_wait(&mon->readers, &mon->mutex);
    printf("reader %lu woke up\n", pthread_self());
    --mon->rwait;
  }

  assert(mon->wcount == 0 || mon->wid == pthread_self());
  ++mon->rcount;
  err = pthread_mutex_unlock(&mon->mutex);

  return err;
}
int reader_exit(Monitor* mon)
{
  int err;

  if (!mon)
    return 0;

  err = pthread_mutex_lock(&mon->mutex);
  --mon->rcount;
  assert(mon->wcount == 0 || mon->wid == pthread_self());
  
  if (mon->rcount == 0 && mon->wwait > 0)
    err = pthread_cond_signal(&mon->writers);
  else
    err = pthread_cond_broadcast(&mon->readers);

  err = pthread_mutex_unlock(&mon->mutex);
  
  return err;
}

/**
 * This is how the editing thread will enter directories. Depending on whether
 * this is their destination dir (`islast`) they will either be treated as a
 * writer or as a reader.
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
    writer_exit(&source_parent->monit);
    writer_exit(&target_parent->monit);
    writer_exit(&lca->monit);
    return ENOENT;
  }
  
  source_dir = hmap_get(source_parent->subdirs, source_dir_name);

  if (!source_dir) {
    writer_exit(&source_parent->monit);
    writer_exit(&target_parent->monit);
    writer_exit(&lca->monit);
    return ENOENT;
  }

  if (hmap_get(target_parent->subdirs, target_dir_name)) {
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
