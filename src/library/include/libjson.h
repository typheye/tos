/**
 ******************************************************************************
 * @file    libjson.h
 * @author  Typheye
 * @brief   Libjson interface.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021-2026 Typheye. All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#ifndef LIBJSON_H
#define LIBJSON_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Strip HTTP headers from a raw response, returning the JSON body.
 *         Looks for "\r\n\r\n" (end of headers) or the first '{' character.
 * @param  http_response  Raw HTTP response string (may be NULL)
 * @return Pointer into http_response where JSON body starts,
 *         or the original pointer if no header boundary found.
 */
const char *json_extract_body(const char *http_response);

/**
 * @brief  Find the position right after "key": in a JSON string.
 *         Example: json_find("{\"a\":1}", "a") returns pointer to "1".
 * @param  buf  JSON string (must be null-terminated)
 * @param  key  Key to search for (without quotes)
 * @return Pointer to the value portion, or NULL if key not found.
 */
const char *json_find(const char *buf, const char *key);

/**
 * @brief  Extract an integer value for a given key.
 * @param  buf         JSON string
 * @param  key         Key to search for
 * @param  default_val Value returned if key not found or parse error
 * @return Parsed integer, or default_val on failure.
 */
int json_get_int(const char *buf, const char *key, int default_val);

/**
 * @brief  Extract a string value for a given key.
 *         Handles basic escape sequences: \n \r \t \\
 * @param  buf    JSON string
 * @param  key    Key to search for
 * @param  out    Output buffer (will be null-terminated)
 * @param  outsz  Size of output buffer
 * @return true if key was found and string extracted, false otherwise.
 */
bool json_get_str(const char *buf, const char *key, char *out, int outsz);

/**
 * @brief  Extract a boolean value for a given key.
 * @param  buf         JSON string
 * @param  key         Key to search for
 * @param  default_val Value returned if key not found
 * @return true/false from JSON, or default_val on failure.
 */
bool json_get_bool(const char *buf, const char *key, bool default_val);

#ifdef __cplusplus
}
#endif

#endif /* LIBJSON_H */
