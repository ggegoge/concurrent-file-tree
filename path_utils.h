
#ifndef _PATH_UTILS_H_
#define _PATH_UTILS_H_

#include <stdbool.h>

#include "HashMap.h"

/** Max length of path (excluding terminating null character). */
#define MAX_PATH_LEN 4095

/** Max length of folder name (excluding terminating null character). */
#define MAX_DIR_NAME_LEN 255

/**
 * Return whether a path is valid.  Valid paths are '/'-separated sequences of
 * folder names, always starting and ending with '/'.  Valid paths have length
 * at most MAX_PATH_LEN (and at least 1). Valid folder names are are
 * sequences of 'a'-'z' ASCII characters, of length from 1 to
 * MAX_DIR_NAME_LEN. */
bool is_path_valid(const char* path);

/**
 * Return the subpath obtained by removing the first component.
 * Args:
 * - `path`: should be a valid path (see `is_path_valid`).
 *
 * - `component`: if not NULL, should be a buffer of size at least
 *    MAX_DIR_NAME_LEN + 1.  Then the first component will be copied there
 *    (without any '/' characters).
 * If path is "/", returns NULL and leaves `component` unchanged.
 * Otherwise the returns a pointer into `path`, representing a valid subpath.
 *
 * This can be used to iterate over all components of a path:
 *     char component[MAX_DIR_NAME_LEN + 1];
 *     const char* subpath = path;
 *     while (subpath = split_path(subpath, component))
 *         printf("%s", component); */
const char* split_path(const char* path, char* component);

/**
 * Return a copy of the subpath obtained by removing the last component.
 * The caller should free the result, unless it is NULL.
 * Args:
 * - `path`: should be a valid path (see `is_path_valid`).
 * - `component`: if not NULL, should be a buffer of size at least
 *    MAX_DIR_NAME_LEN + 1.  Then the last component will be copied there
 *    (without any '/' characters).
 * If path is "/", returns NULL and leaves `component` unchanged.
 * Otherwise the result is a valid path. */
char* make_path_to_parent(const char* path, char* component);

/**
 * Return an array containing all keys, lexicographically sorted.
 * The result is null-terminated.
 * Keys are not copied, they are only valid as long as the map.
 * The caller should free the result. */
const char** make_map_contents_array(HashMap* map);

/**
 * Return a string containing all keys in map, sorted, comma-separated.
 * The result has no trailing comma. An empty map yields an empty string.
 * The caller should free the result. */
char* make_map_contents_string(HashMap* map);

/**
 * Test whether `path1` is a proper subpath of `path2`.
 * Assumes both are valid paths. */
bool is_proper_subpath(const char* path1, const char* path2);

/**
 * Find the lowest common ancestor of two paths. Will save path to the lca under
 * `lca`, assumes it is big enough and that `p1` and `p2` are both valid. It
 * also will have `p1lca` and `p2lca` pointing at the parts after the lca.
 *
 * The result should be freed by the caller.
 */
char* path_lca(const char* p1, const char* p2,
               const char** p1lca, const char** p2lca);

#endif  /* _PATH_UTILS_H_ */
