#ifndef _TREE_H_
#define _TREE_H_

/* CUSTOM ERROR CODES */

/** Tried to move a directory into its descendant. */
#define ESUBPATH -3

/** Our tree type */
typedef struct Tree Tree;

/** Create a new heap-allocated tree. */
Tree* tree_new();

/** Free a tree's memory. */
void tree_free(Tree*);

/** Return a comma separated list with all the subdirectories under a path. */
char* tree_list(Tree* tree, const char* path);

/** Create a new subdirectory. */
int tree_create(Tree* tree, const char* path);

/** Remove a subdirectory. */
int tree_remove(Tree* tree, const char* path);

/** Move a soruce subdirectory to a new target location. */
int tree_move(Tree* tree, const char* source, const char* target);

#endif  /* _TREE_H_ */
