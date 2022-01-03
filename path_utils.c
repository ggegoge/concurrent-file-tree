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
  const char* name_start;
  const char* name_end;

  if (len == 0 || len > MAX_PATH_LEN)
    return false;

  if (path[0] != '/' || path[len - 1] != '/')
    return false;

  /* Start of current path component, just after '/'. */
  name_start = path + 1;

  while (name_start < path + len) {
    /* End of current path component, at '/'. */
    name_end = strchr(name_start, '/');

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
  /* Pointer to second '/' character. */
  const char* subpath = strchr(path + 1, '/');
  size_t len;

  /* Path is "/". */
  if (!subpath)
    return NULL;

  if (component) {
    len = subpath - (path + 1);
    assert(len >= 1 && len <= MAX_DIR_NAME_LEN);
    strncpy(component, path + 1, len);
    component[len] = '\0';
  }

  return subpath;
}

char* make_path_to_parent(const char* path, char* component)
{
  size_t len = strlen(path);
  size_t subpath_len;
  char* result;
  size_t component_len;

  /* Path is "/". */
  if (len == 1)
    return NULL;

  const char* p = path + len - 2; // Point before final '/' character.

  // Move p to last-but-one '/' character.
  while (*p != '/')
    p--;

  subpath_len = p - path + 1; // Include '/' at p.
  result = malloc(subpath_len + 1); // Include terminating null character.
  strncpy(result, path, subpath_len);
  result[subpath_len] = '\0';

  if (component) {
    component_len = len - subpath_len - 1; // Skip final '/' as well.
    assert(component_len >= 1 && component_len <= MAX_DIR_NAME_LEN);
    strncpy(component, p + 1, component_len);
    component[component_len] = '\0';
  }

  return result;
}

/**
 * A wrapper for using strcmp in qsort.
 * The arguments here are actually pointers to (const char*).
 */
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

  // Set last array element to NULL.
  *key = NULL;
  qsort(result, n_keys, sizeof(char*), compare_string_pointers);
  return result;
}

char* make_map_contents_string(HashMap* map)
{
  const char** keys = make_map_contents_array(map);
  char* result;
  char* position;
  size_t keylen;
  // Including ending null character.
  size_t result_size = 0;

  for (const char** key = keys; *key; ++key)
    result_size += strlen(*key) + 1;

  // Return empty string if map is empty.
  if (!result_size) {
    free(keys);
    /* Note we can't just return "", as it can't be free'd. */
    char* result = malloc(1);
    *result = '\0';
    return result;
  }

  result = malloc(result_size);
  position = result;

  for (const char** key = keys; *key; ++key) {
    keylen = strlen(*key);
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

bool is_proper_subpath(const char* path1, const char* path2)
{
  char comp1[MAX_DIR_NAME_LEN + 1];
  char comp2[MAX_DIR_NAME_LEN + 1];
  const char* sub1 = path1;
  const char* sub2 = path2;

  for (;;) {
    sub1 = split_path(sub1, comp1);
    sub2 = split_path(sub2, comp2);

    /* if path1 runs out before path2 and it had equal components up to now
     * then it is indeed a subpath. We want proper subpath -> no eq */
    if (!sub1 && !sub2)
      return false;
    else if (!sub1)
      return true;
    else if (!sub2)
      return false;
    else if (strcmp(comp1, comp2) != 0)
      return false;
  }
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
