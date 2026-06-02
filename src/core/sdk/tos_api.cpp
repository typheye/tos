/**
 * @file    tos_api.cpp
 * @brief   Cloud API — upgrade check via HTTP GET + JSON parse
 *
 * Uses network_manager for HTTP and libjson for JSON parsing.
 */

#include "tos_api.h"
#include "../include/config.h"
#include "core/manager/include/network_manager.h"
#include "include/libjson.h"
#include "syslog.h"
#include <cstdio>
#include <cstring>

bool TosApi_CheckUpgrade(TosUpgradeInfo *info) {
  if (!info) return false;

  /* If ESP8266 is hard-disabled, bail out immediately */
  if (Net_IsHardDisabled()) {
    LOG_W("TAPI", "ESP8266 is hard-disabled — cannot check upgrade");
    return false;
  }

  memset(info, 0, sizeof(*info));

  char host[64];
  snprintf(host, sizeof(host), "%s", TOS_API_HOST);

  char path[128];
  snprintf(path, sizeof(path),
           "%s?model=%s&version=%s&version_code=%lu",
           TOS_API_PATH, CFG_MODEL, CFG_TOS_VERSION,
           (unsigned long)CFG_VERSION_CODE);

  LOG_I("TAPI", "Checking update: %s:%d%s", host, TOS_API_PORT, path);

  /* Use network_manager for the HTTP GET — LED rules applied internally */
  char resp[1024];
  memset(resp, 0, sizeof(resp));

  if (!Net_HttpGet(host, TOS_API_PORT, path, resp, sizeof(resp), 15000)) {
    LOG_E("TAPI", "HTTP request failed");
    return false;
  }

  LOG_D("TAPI", "Raw: %.200s", resp);

  /* Parse JSON body with libjson */
  const char *body = json_extract_body(resp);

  /* Check response code */
  int code = json_get_int(body, "code", -1);
  if (code != 0) {
    LOG_E("TAPI", "API error code: %d", code);
    return false;
  }

  /* Parse "data" -> "has_update" */
  info->has_update = json_get_bool(body, "has_update", false);

  /* Parse "latest" fields */
  const char *latest = strstr(body, "\"latest\"");
  if (latest) {
    json_get_str(latest, "version", info->latest_version, sizeof(info->latest_version));
    info->latest_version_code = json_get_int(latest, "version_code", 0);
    json_get_str(latest, "build", info->latest_build, sizeof(info->latest_build));
    json_get_str(latest, "patch", info->latest_patch, sizeof(info->latest_patch));
    json_get_str(latest, "url", info->latest_url, sizeof(info->latest_url));
    info->latest_size = json_get_int(latest, "size", 0);
    json_get_str(latest, "sha256", info->latest_sha256, sizeof(info->latest_sha256));
  }

  LOG_D("TAPI", "Latest: ver='%s' build='%s' patch='%s' size=%d",
        info->latest_version, info->latest_build, info->latest_patch,
        info->latest_size);
  LOG_I("TAPI", "Update check done. has_update=%d", info->has_update);
  return true;
}
