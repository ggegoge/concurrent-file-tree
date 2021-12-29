#include "path_utils.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool is_path_valid(const char* path)
{
  size_t len = strlen(path);

  if (len == 0 || len > MAX_PATH_LEN)
    return false;

  if (path[0] != '/' || path[len - 1] != '/')
    return false;

  const char* name_start = path +
                           1; // Start of current path component, just after '/'.

  while (name_start < path + len) {
    char* name_end = strchr(name_start,
                            '/'); // End of current path component, at '/'.

    if (!name_end || name_end == name_start ||
        name_end > name_start + MAX_DIR_NAME_LEN)
      return false;

    for (const char* p = name_start; p != name_end; ++p)
      if (*p < 'a' || *p > 'z')
        return false;

    name_start = name_end + 1;
  }

  return true;
}

const char* split_path(const char* path, char* component)
{
  const char* subpath = strchr(path + 1, '/'); // Pointer to second '/' character.

  if (!subpath) // Path is "/".
    return NULL;

  if (component) {
    int len = subpath - (path + 1);
    assert(len >= 1 && len <= MAX_DIR_NAME_LEN);
    strncpy(component, path + 1, len);
    component[len] = '\0';
  }

  return subpath;
}

char* make_path_to_parent(const char* path, char* component)
{
  size_t len = strlen(path);

  if (len == 1) // Path is "/".
    return NULL;

  const char* p = path + len - 2; // Point before final '/' character.

  // Move p to last-but-one '/' character.
  while (*p != '/')
    p--;

  size_t subpath_len = p - path + 1; // Include '/' at p.
  char* result = malloc(subpath_len + 1); // Include terminating null character.
  strncpy(result, path, subpath_len);
  result[subpath_len] = '\0';

  if (component) {
    size_t component_len = len - subpath_len - 1; // Skip final '/' as well.
    assert(component_len >= 1 && component_len <= MAX_DIR_NAME_LEN);
    strncpy(component, p + 1, component_len);
    component[component_len] = '\0';
  }

  return result;
}

// A wrapper for using strcmp in qsort.
// The arguments here are actually pointers to (const char*).
static int compare_string_pointers(const void* p1, const void* p2)
{
  return strcmp(*(const char**)p1, *(const char**)p2);
}

const char** make_map_contents_array(HashMap* map)
{
  size_t n_keys = hmap_size(map);
  const char** result = calloc(n_keys + 1, sizeof(char*));
  HashMapIterator it = hmap_iterator(map);
  const char** key = result;
  void* value = NULL;

  while (hmap_next(map, &it, key, &value)) {
    key++;
  }

  *key = NULL; // Set last array element to NULL.
  qsort(result, n_keys, sizeof(char*), compare_string_pointers);
  return result;
}

char* make_map_contents_string(HashMap* map)
{
  const char** keys = make_map_contents_array(map);

  unsigned int result_size = 0; // Including ending null character.

  for (const char** key = keys; *key; ++key)
    result_size += strlen(*key) + 1;

  // Return empty string if map is empty.
  if (!result_size) {
    free(keys);
    // Note we can't just return "", as it can't be free'd.
    char* result = malloc(1);
    *result = '\0';
    return result;
  }

  char* result = malloc(result_size);
  char* position = result;

  for (const char** key = keys; *key; ++key) {
    size_t keylen = strlen(*key);
    assert(position + keylen <= result + result_size);
    strcpy(position, *key); // NOLINT: array size already checked.
    position += keylen;
    *position = ',';
    position++;
  }

  position--;
  *position = '\0';
  free(keys);
  return result;
}

/* return: p1 \subseteq p2? */
bool is_subpath(const char* path1, const char* path2)
{
  char comp1[MAX_DIR_NAME_LEN + 1];
  char comp2[MAX_DIR_NAME_LEN + 1];
  const char* sub1 = path1;
  const char* sub2 = path2;

  for (;;) {
    sub1 = split_path(sub1, comp1);
    sub2 = split_path(sub2, comp2);

    /* if path1 runs out before path2 and it had equal components up to now
     * then it is indeed a subpath. */
    if (!sub1)
      return true;
    else if (!sub2)
      return false;
    else if (strcmp(comp1, comp2) != 0)
      return false;
  }
}

char* path_lca(const char* path1, const char* path2)
{
  size_t i;
  size_t len1 = strlen(path1);
  size_t len2 = strlen(path2);
  size_t max_len = len1 > len2 ? len1 : len2;
  for (i = 0; i < max_len && path1[i] == path2[i]; ++i)
    ;

  return strndup(path1, i);
}

char* path_lca_move(const char* p1, const char* p2,
                    const char** p1lca, const char** p2lca)
{
  size_t i;
  size_t len1 = strlen(p1);
  size_t len2 = strlen(p2);
  size_t max_len = len1 > len2 ? len1 : len2;
  for (i = 0; i < max_len && p1[i] == p2[i]; ++i)
    ;

  *p1lca = p1 + i - 1;
  *p2lca = p2 + i - 1;
  return strndup(p2, i);
}
