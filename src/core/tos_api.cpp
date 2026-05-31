/**
 * @file    tos_api.cpp
 * @brief   Cloud API — HTTP GET + JSON parse
 */

#include "include/tos_api.h"
#include "include/config.h"
#include "hardware/include/esp8266.hpp"
#include "syslog.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

/* Minimal JSON integer extractor: find "key":<number> */
static int json_get_int(const char *buf, const char *key) {
  char search[32];
  snprintf(search, sizeof(search), "\"%s\":", key);
  const char *p = strstr(buf, search);
  if (!p) return 0;
  p += strlen(search);
  while (*p == ' ' || *p == '\t') p++;
  return atoi(p);
}

/* Minimal JSON string extractor: find "key":"value" */
static void json_get_str(const char *buf, const char *key, char *out, int outsz) {
  char search[32];
  snprintf(search, sizeof(search), "\"%s\":\"", key);
  const char *p = strstr(buf, search);
  if (!p) { out[0] = '\0'; return; }
  p += strlen(search);
  int i = 0;
  while (*p && *p != '"' && i < outsz - 1) out[i++] = *p++;
  out[i] = '\0';
}

/* Minimal JSON bool extractor */
static bool json_get_bool(const char *buf, const char *key) {
  char search[32];
  snprintf(search, sizeof(search), "\"%s\":", key);
  const char *p = strstr(buf, search);
  if (!p) return false;
  p += strlen(search);
  while (*p == ' ' || *p == '\t') p++;
  return (*p == 't' || *p == 'T');
}

bool TosApi_CheckUpgrade(TosUpgradeInfo *info) {
  if (!info) return false;
  memset(info, 0, sizeof(*info));

  char host[64];
  snprintf(host, sizeof(host), "%s", TOS_API_HOST);

  char path[128];
  snprintf(path, sizeof(path),
           "%s?model=%s&version=%s&version_code=%lu",
           TOS_API_PATH, CFG_MODEL, CFG_TOS_VERSION,
           (unsigned long)CFG_VERSION_CODE);

  LOG_I("TAPI", "Checking update: %s:%d%s", host, TOS_API_PORT, path);

  /* Ensure single-connection mode */
  ESP8266_SendCommand("AT+CIPMUX=0", "OK", 2000);
  HAL_Delay(100);

  /* Test DNS resolution */
  {
    char cmd[80];
    snprintf(cmd, sizeof(cmd), "AT+PING=\"%s\"", host);
    bool dns_ok = ESP8266_SendCommand(cmd, "+", 5000);
    LOG_I("TAPI", "DNS test: %s", dns_ok ? "OK" : "FAIL");
    if (dns_ok) {
      /* PING succeeded, close any half-open connection */
      ESP8266_SendCommand("AT+CIPCLOSE", "", 1000);
      HAL_Delay(200);
    }
  }

  /* TCP connect */
  {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d", host, TOS_API_PORT);
    if (!ESP8266_SendCommand(cmd, "CONNECT", 15000)) {
      LOG_E("TAPI", "TCP connect failed (DNS may have failed)");
      return false;
    }
  }
  HAL_Delay(500);

  /* HTTP GET request */
  char req[384];
  int rlen = snprintf(req, sizeof(req),
    "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
    path, host);

  if (!ESP8266_SendData((const uint8_t *)req, rlen)) {
    LOG_E("TAPI", "HTTP send failed");
    ESP8266_SendCommand("AT+CIPCLOSE", "", 2000);
    return false;
  }

  /* Wait for response */
  HAL_Delay(1500);
  ESP8266_SendCommand("AT+CIPCLOSE", "", 2000);

  const char *buf = esp8266.getRxBuffer();
  if (!buf || !*buf) {
    LOG_E("TAPI", "Empty response");
    return false;
  }

  LOG_D("TAPI", "Raw: %.200s", buf);

  /* Parse JSON body (after HTTP headers) */
  const char *body = strstr(buf, "\r\n\r\n");
  if (!body) body = buf;
  else body += 4;

  /* Check response code */
  int code = json_get_int(body, "code");
  if (code != 0) {
    LOG_E("TAPI", "API error code: %d", code);
    return false;
  }

  /* Parse "data" -> "has_update" */
  info->has_update = json_get_bool(body, "has_update");

  /* Parse "latest" fields */
  const char *latest = strstr(body, "\"latest\":");
  if (latest) {
    json_get_str(latest, "version",      info->latest_version, sizeof(info->latest_version));
    info->latest_version_code = json_get_int(latest, "version_code");
    json_get_str(latest, "build",        info->latest_build, sizeof(info->latest_build));
    json_get_str(latest, "patch",        info->latest_patch, sizeof(info->latest_patch));
    json_get_str(latest, "url",          info->latest_url, sizeof(info->latest_url));
    info->latest_size = json_get_int(latest, "size");
    json_get_str(latest, "sha256",       info->latest_sha256, sizeof(info->latest_sha256));
  }

  LOG_I("TAPI", "Update check done. has_update=%d", info->has_update);
  return true;
}
