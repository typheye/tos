/**
 * @file    libjson.c
 * @brief   Lightweight JSON parser — no malloc, C-compatible
 */

#include "include/libjson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── helpers ──────────────────────────────────────────────────── */

static void skip_ws(const char **p) {
  while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n')
    ++(*p);
}

/* ── public API ───────────────────────────────────────────────── */

const char *json_extract_body(const char *http) {
  if (!http || !*http) return http;
  const char *body = strstr(http, "\r\n\r\n");
  if (body) {
    body += 4;
    return body;
  }
  body = strchr(http, '{');
  return body ? body : http;
}

const char *json_find(const char *buf, const char *key) {
  if (!buf || !key) return NULL;

  char search[48];
  int n = snprintf(search, sizeof(search), "\"%s\"", key);
  if (n <= 0 || n >= (int)sizeof(search)) return NULL;

  const char *p = strstr(buf, search);
  if (!p) return NULL;

  p += n;           /* skip past "\"key\"" */
  skip_ws(&p);
  if (*p != ':') return NULL;
  ++p;              /* skip ':' */
  skip_ws(&p);
  return p;
}

int json_get_int(const char *buf, const char *key, int default_val) {
  const char *p = json_find(buf, key);
  if (!p) return default_val;
  return atoi(p);
}

bool json_get_str(const char *buf, const char *key, char *out, int outsz) {
  if (!out || outsz <= 0) return false;
  out[0] = '\0';

  const char *p = json_find(buf, key);
  if (!p) return false;
  if (*p != '"') return false;
  ++p; /* skip opening quote */

  int i = 0;
  while (*p && *p != '"' && i < outsz - 1) {
    if (*p == '\\' && p[1]) {
      ++p;
      switch (*p) {
      case 'n':  out[i++] = '\n'; break;
      case 'r':  out[i++] = '\r'; break;
      case 't':  out[i++] = '\t'; break;
      case '\\': out[i++] = '\\'; break;
      case '"':  out[i++] = '"';  break;
      default:   out[i++] = *p;   break;
      }
      ++p;
    } else {
      out[i++] = *p++;
    }
  }
  out[i] = '\0';
  return true;
}

bool json_get_bool(const char *buf, const char *key, bool default_val) {
  const char *p = json_find(buf, key);
  if (!p) return default_val;
  return (*p == 't' || *p == 'T');
}
