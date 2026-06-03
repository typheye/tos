/**
 ******************************************************************************
 * @file    tos_api.cpp
 * @author  Typheye
 * @brief   TOS cloud API client implementation.
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

#include "include/tos_api.h"


extern Buzzer buzzer1;

#define TOS_HEARTBEAT_PATH "/v1/device/heartbeat"
#define TOS_HEARTBEAT_MS   3000U
#define TOS_RETRY_MS       5000U
#define TOS_START_DELAY_MS 2500U
#define TOS_CMD_GET_TIMEOUT_MS 3000U
#define TOS_NET_STUCK_MS       30000U
#define TOS_RSSI_REFRESH_MS     60000U
#define TOS_RSSI_RETRY_MS       15000U
#define TOS_RSSI_IDLE_BUDGET_MS 1800U
#define TOS_ACK_MAX_CMDS   5
#define TOS_MAX_CMD_OBJ     448U

enum TosApiPhase {
  TOS_PHASE_IDLE = 0,
  TOS_PHASE_HEARTBEAT,
  TOS_PHASE_ACK,
};

static TosApiPhase g_phase = TOS_PHASE_IDLE;
static uint32_t g_next_heartbeat_ms = 0;
static bool g_initialized = false;
static bool g_reboot_after_ack = false;
static char g_device_id[13];
static char g_ack_body[640];
static char g_ack_path[80];
static char g_last_ip[24] = "0.0.0.0";
static uint32_t g_last_ip_refresh_ms = 0;
static uint8_t g_empty_hb_cycles = 0;
static bool g_last_cmd_parse_malformed = false;
static bool g_last_hb_malformed = false;
static uint32_t g_last_full_hb_ms = 0;
static uint8_t g_full_hb_boot_count = 0;
static uint32_t g_last_cloud_ok_ms = 0;
static uint8_t g_transport_fail_count = 0;
static bool g_cloud_was_online = false;
static bool g_offline_for_this_boot = false;
static int g_last_rssi = 0;
static uint32_t g_last_rssi_refresh_ms = 0;

static bool time_due(uint32_t now, uint32_t target) {
  return (int32_t)(now - target) >= 0;
}

static void schedule_heartbeat(uint32_t delay_ms) {
  g_next_heartbeat_ms = HAL_GetTick() + delay_ms;
}

static void build_device_id(void) {
  uint32_t uid0 = HAL_GetUIDw0();
  uint32_t uid1 = HAL_GetUIDw1();
  uint32_t uid2 = HAL_GetUIDw2();
  uint32_t a = uid0 ^ (uid2 << 1);
  uint32_t b = uid1 ^ (uid2 >> 1);
  snprintf(g_device_id, sizeof(g_device_id), "%08lx%04lx",
           (unsigned long)a, (unsigned long)(b & 0xFFFFU));
}

static bool wlan_enabled(void) {
  return SM_Wlan_On();
}

static bool online_intended_config(void) {
  /* Cloud-online mode is a user/config decision, not merely "ESP answers AT".
   * Require all three persisted WLAN conditions requested by the product policy:
   *   1) WLAN switch ON
   *   2) auto-connect ON
   *   3) at least one non-empty saved network
   */
  if (!SM_Wlan_On() || !SM_Wlan_AutoConn()) return false;
  uint8_t n = SM_Saved_Count();
  for (uint8_t i = 0; i < n; ++i) {
    const SM_SavedNet_t *net = SM_Saved_Get(i);
    if (net && net->ssid[0]) return true;
  }
  return false;
}

static bool network_ready(void) {
  if (!wlan_enabled()) return false;
  if (Net_IsHardDisabled()) return false;
  int state = ESP8266_GetState();
  /* Some ESP8266 AT firmwares stay in STATUS:2 while TCP requests still work.
   * Once cloud has been reached in this boot, keep treating WLAN as usable
   * until transport failures prove otherwise. */
  return state == 3 || ESP8266_IsConnected() || g_cloud_was_online;
}

static bool cloud_offline_mode(void) {
  /* High-priority offline gate.  If the product is not configured for online
   * operation, or it failed to join a saved WLAN during this boot, it is simply
   * offline.  Do not probe cloud and do not recover/re-init ESP8266 here. */
  if (g_offline_for_this_boot) return true;
  if (!online_intended_config()) return true;
  if (!g_cloud_was_online && !network_ready()) return true;
  return false;
}

static int response_code(const char *resp) {
  const char *body = json_extract_body(resp);
  return json_get_int(body, "c", json_get_int(body, "code", -1));
}

static const char *response_body(const char *resp) {
  return json_extract_body(resp);
}

static void note_transport_failure(const char *where);

static bool rssi_valid(int rssi) {
  return rssi < 0 && rssi >= -127;
}

static int cached_rssi(bool refresh) {
  (void)refresh;
  if (!wlan_enabled()) return 0;
  return rssi_valid(g_last_rssi) ? g_last_rssi : -99;
}

static void maybe_refresh_rssi(uint32_t now) {
  if (!wlan_enabled() || g_offline_for_this_boot || !network_ready()) return;
  if (g_ack_body[0]) return;
  if (g_last_rssi_refresh_ms != 0U) {
    uint32_t interval = rssi_valid(g_last_rssi) ? TOS_RSSI_REFRESH_MS : TOS_RSSI_RETRY_MS;
    if ((uint32_t)(now - g_last_rssi_refresh_ms) < interval) return;
  }

  int rssi = 0;
  g_last_rssi_refresh_ms = now;
  if (ESP8266_GetRSSI(&rssi) && rssi_valid(rssi)) {
    if (g_last_rssi != rssi) {
      LOG_D("TAPI", "RSSI cache: %d dBm", rssi);
    }
    g_last_rssi = rssi;
  } else if (!rssi_valid(g_last_rssi)) {
    g_last_rssi = -99;
  }
}

static bool build_heartbeat_body(char *out, size_t out_sz) {
  if (!out || out_sz == 0) return false;
  const char *expr = EmotionManager_GetReportExpression();
  if (!expr || !expr[0]) expr = "idle";

  bool net_ok = network_ready();
  const char *wifi = net_ok ? "connected" : "disconnected";
  const char *ip = "0.0.0.0";

  if (net_ok) {
    uint32_t tick = HAL_GetTick();
    if (g_last_ip[0] == '\0') snprintf(g_last_ip, sizeof(g_last_ip), "0.0.0.0");
    g_last_ip_refresh_ms = tick;
    ip = g_last_ip;
  }

  uint32_t now = HAL_GetTick();
  bool full = (g_full_hb_boot_count < 3U) ||
              ((uint32_t)(now - g_last_full_hb_ms) >= 60000U);
  int rssi = wlan_enabled() ? cached_rssi(full) : 0;
  int n;
  if (full) {
    n = snprintf(out, out_sz,
                 "{\"di\":\"%s\",\"v\":\"1\",\"vc\":%lu,"
                 "\"b\":\"%s\",\"m\":\"%s\",\"hv\":\"%s\","
                 "\"up\":%lu,\"w\":\"%s\",\"rssi\":%d,"
                 "\"ip\":\"%s\",\"e\":\"%s\"}",
                 g_device_id,
                 (unsigned long)CFG_VERSION_CODE,
                 CFG_BUILD, CFG_MODEL, CFG_HW_REV,
                 (unsigned long)(now / 1000U),
                 wifi, rssi, ip, expr);
    if (n > 0 && n < (int)out_sz) {
      g_last_full_hb_ms = now;
      if (g_full_hb_boot_count < 255U) g_full_hb_boot_count++;
    }
  } else {
    n = snprintf(out, out_sz,
                 "{\"di\":\"%s\",\"up\":%lu,\"w\":\"%s\",\"rssi\":%d,\"ip\":\"%s\",\"e\":\"%s\"}",
                 g_device_id, (unsigned long)(now / 1000U), wifi, rssi, ip, expr);
  }
  return n > 0 && n < (int)out_sz;
}

static const char *find_array_end(const char *arr) {
  if (!arr || *arr != '[') return nullptr;
  int depth = 0;
  bool in_str = false;
  bool esc = false;
  for (const char *p = arr; *p; ++p) {
    char c = *p;
    if (in_str) {
      if (esc) esc = false;
      else if (c == '\\') esc = true;
      else if (c == '"') in_str = false;
      continue;
    }
    if (c == '"') in_str = true;
    else if (c == '[') depth++;
    else if (c == ']') {
      depth--;
      if (depth == 0) return p;
    }
  }
  return nullptr;
}

static bool next_json_object(const char **cursor, const char *end,
                             char *out, size_t out_sz) {
  if (!cursor || !*cursor || !end || !out || out_sz == 0) return false;

  const char *p = *cursor;
  while (p < end && *p != '{') p++;
  if (p >= end) return false;

  const char *start = p;
  int depth = 0;
  bool in_str = false;
  bool esc = false;
  for (; p < end && *p; ++p) {
    char c = *p;
    if (in_str) {
      if (esc) esc = false;
      else if (c == '\\') esc = true;
      else if (c == '"') in_str = false;
      continue;
    }

    if (c == '"') in_str = true;
    else if (c == '{') depth++;
    else if (c == '}') {
      depth--;
      if (depth == 0) {
        size_t len = (size_t)(p - start + 1);
        if (len >= out_sz) len = out_sz - 1;
        memcpy(out, start, len);
        out[len] = '\0';
        *cursor = p + 1;
        return true;
      }
    }
  }
  return false;
}

static bool append_ack(char *buf, size_t buf_sz, int *count,
                       const char *id, bool ok, const char *err) {
  if (!buf || !count || !id || !id[0]) return false;
  if (!err) err = "";

  size_t len = strlen(buf);
  int n = snprintf(buf + len, buf_sz - len,
                   "%s{\"id\":\"%s\",\"ok\":%s,\"err\":\"%s\"}",
                   (*count > 0) ? "," : "", id, ok ? "true" : "false", err);
  if (n <= 0 || (size_t)n >= buf_sz - len) return false;
  (*count)++;
  return true;
}

static bool json_get_cmd_str(const char *obj, const char *key,
                             char *out, int outsz) {
  if (json_get_str(obj, key, out, outsz)) return true;

  const char *params = json_find(obj, "params");
  if (params && *params == '{') {
    return json_get_str(params, key, out, outsz);
  }
  return false;
}

static int json_get_cmd_int(const char *obj, const char *key, int defv) {
  int v = json_get_int(obj, key, 0x7FFFFFFF);
  if (v != 0x7FFFFFFF) return v;

  const char *params = json_find(obj, "params");
  if (params && *params == '{') {
    return json_get_int(params, key, defv);
  }
  return defv;
}

static const char *normalize_expr_alias(char *expr) {
  if (!expr || !expr[0]) return expr;
  if (strcmp(expr, "angry") == 0 || strcmp(expr, "mad") == 0) {
    strcpy(expr, "annoyed");
  } else if (strcmp(expr, "normal") == 0 || strcmp(expr, "neutral") == 0) {
    strcpy(expr, "idle");
  } else if (strcmp(expr, "surprise") == 0) {
    strcpy(expr, "surprised");
  } else if (strcmp(expr, "cute") == 0) {
    strcpy(expr, "shy");
  }
  return expr;
}

static bool action_is(const char *action, const char *a, const char *b = nullptr,
                      const char *c = nullptr) {
  return action && ((a && strcmp(action, a) == 0) ||
                    (b && strcmp(action, b) == 0) ||
                    (c && strcmp(action, c) == 0));
}

static bool execute_command(const char *obj, bool *reboot_after_ack,
                            const char **err_out) {
  char action[40];
  action[0] = '\0';
  if (!json_get_str(obj, "action", action, sizeof(action))) {
    *err_out = "missing action";
    return false;
  }

  if (action_is(action, "set_expression", "expression", "set_emotion") ||
      action_is(action, "set_expresion")) {
    char expr[16];
    expr[0] = '\0';

    /* Backend commands are shaped as:
     *   {"id":"...","action":"set_expression","params":{"expr":"happy"}}
     * Accept both top-level and nested params, and both names: expression/expr.
     */
    if (!json_get_cmd_str(obj, "expression", expr, sizeof(expr)) &&
        !json_get_cmd_str(obj, "expr", expr, sizeof(expr))) {
      *err_out = "missing expression";
      return false;
    }

    normalize_expr_alias(expr);
    if (strcmp(expr, "auto") == 0 || strcmp(expr, "idle") == 0) {
      EmotionManager_SetAuto();
      return true;
    }

    if (!EmotionManager_SetExpression(expr)) {
      *err_out = "bad expression";
      return false;
    }
    return true;
  }

  if (action_is(action, "set_auto_expression", "auto_expression", "emotion_auto")) {
    EmotionManager_SetAuto();
    return true;
  }

  if (action_is(action, "bleep", "beep", "buzzer")) {
    int duration = json_get_cmd_int(obj, "duration", 80);
    if (duration < 20) duration = 20;
    if (duration > 300) duration = 300;

    bool respect_mute = json_get_cmd_int(obj, "respect_mute", 0) != 0;
    if (respect_mute) buzzer1.beep((uint32_t)duration);
    else buzzer1.forceBeep((uint32_t)duration);
    return true;
  }

  if (action_is(action, "sync_time", "time_sync", "ntp_sync")) {
    /* Blocking but explicit: this command is user-triggered from cloud UI.
     * It runs after the heartbeat HTTP request has completed, before ACK POST.
     */
    SysWatchdog_FeedNow();
    bool ok = SysTime_Sync();
    SysWatchdog_FeedNow();
    if (!ok) {
      *err_out = "time sync failed";
      return false;
    }
    return true;
  }

  if (action_is(action, "ota_check", "check_update", "upgrade_check")) {
    TosUpgradeInfo info;
    SysWatchdog_FeedNow();
    bool ok = TosApi_CheckUpgrade(&info);
    SysWatchdog_FeedNow();
    if (!ok) {
      *err_out = "ota check failed";
      return false;
    }
    LOG_I("TAPI", "OTA check: update=%d latest=%s code=%lu",
          info.has_update ? 1 : 0,
          info.latest_version[0] ? info.latest_version : "-",
          (unsigned long)info.latest_version_code);
    return true;
  }

  if (action_is(action, "reboot", "restart", "reset")) {
    *reboot_after_ack = true;
    return true;
  }

  if (action_is(action, "noop", "ping")) {
    return true;
  }

  LOG_W("TAPI", "Unknown command action='%s' obj=%.120s", action, obj ? obj : "");
  *err_out = "unknown action";
  return false;
}

static bool parse_commands_from_body(const char *body, const char *source,
                                     bool *reboot_out) {
  if (reboot_out) *reboot_out = false;
  g_last_cmd_parse_malformed = false;
  if (!body || !*body) return false;

  /* Backend currently wraps payload as {"c":0,"d":{...}}.
   * Commands may appear as d.cmds, cmds, commands, or cs in future builds.
   * Search globally so the wrapper does not matter.
   */
  const char *keys[] = {"\"cmds\"", "\"commands\"", "\"cs\""};
  const char *cmds = nullptr;
  for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
    cmds = strstr(body, keys[i]);
    if (cmds) break;
  }

  if (!cmds) {
    LOG_D("TAPI", "%s: no command array", source ? source : "resp");
    if (source && strcmp(source, "GET") == 0) {
      LOG_D("TAPI", "GET body preview: %.180s", body);
    }
    return false;
  }

  const char *arr = strchr(cmds, '[');
  const char *end = find_array_end(arr);
  if (!arr || !end) {
    g_last_cmd_parse_malformed = true;
    LOG_W("TAPI", "%s: command array malformed", source ? source : "resp");
    LOG_D("TAPI", "cmd area: %.160s", cmds);
    return false;
  }

  g_ack_body[0] = '\0';
  strcpy(g_ack_body, "{\"acks\":[");
  int ack_count = 0;
  bool reboot = false;

  const char *cur = arr + 1;
  char obj[TOS_MAX_CMD_OBJ];
  int parsed = 0;
  while (parsed < TOS_ACK_MAX_CMDS &&
         next_json_object(&cur, end, obj, sizeof(obj))) {
    parsed++;

    char id[24];
    id[0] = '\0';
    if (!json_get_str(obj, "id", id, sizeof(id))) {
      LOG_W("TAPI", "%s: cloud command without id", source ? source : "resp");
      continue;
    }

    char action_dbg[40];
    action_dbg[0] = '\0';
    (void)json_get_str(obj, "action", action_dbg, sizeof(action_dbg));
    LOG_I("TAPI", "%s: command id=%s action=%s", source ? source : "resp", id,
          action_dbg[0] ? action_dbg : "(none)");

    const char *err = "";
    bool ok = execute_command(obj, &reboot, &err);
    append_ack(g_ack_body, sizeof(g_ack_body), &ack_count, id, ok, err);
    LOG_I("TAPI", "Command %s %s", id, ok ? "OK" : err);
  }

  if (parsed == 0) {
    LOG_D("TAPI", "%s: command array empty", source ? source : "resp");
    g_ack_body[0] = '\0';
    return false;
  }

  if (ack_count > 0) {
    size_t len = strlen(g_ack_body);
    if (len + 3 < sizeof(g_ack_body)) {
      strcpy(g_ack_body + len, "]}");
      g_reboot_after_ack = reboot;
      if (reboot_out) *reboot_out = reboot;
      snprintf(g_ack_path, sizeof(g_ack_path),
               "/v1/device/%s/commands/ack", g_device_id);
      LOG_D("TAPI", "Ack body ready, bytes=%u", (unsigned)strlen(g_ack_body));
      return true;
    }
    g_ack_body[0] = '\0';
    LOG_W("TAPI", "Ack body overflow");
  }

  return false;
}

static bool poll_commands_once(void) {
  char path[96];
  snprintf(path, sizeof(path), "/v1/device/%s/commands", g_device_id);

  char resp[2048];
  memset(resp, 0, sizeof(resp));
  LOG_I("TAPI", "Polling commands fallback");
  if (!Net_HttpGet(TOS_API_HOST, TOS_API_PORT, path, resp, sizeof(resp), TOS_CMD_GET_TIMEOUT_MS)) {
    LOG_W("TAPI", "Command fallback GET failed");
    return false;
  }

  int code = response_code(resp);
  if (code != 0) {
    LOG_W("TAPI", "Command fallback API code=%d", code);
    return false;
  }

  bool reboot = false;
  bool has = parse_commands_from_body(response_body(resp), "GET", &reboot);
  if (has) {
    LOG_I("TAPI", "Command fallback got command(s)");
  }
  return has;
}

static bool parse_heartbeat_response(const char *resp) {
  const char *body = response_body(resp);
  int code = response_code(resp);
  if (code != 0) {
    LOG_W("TAPI", "Heartbeat API code=%d", code);
    /* Still try fallback GET later; some proxy errors are transient. */
    return false;
  }

  g_ack_body[0] = '\0';
  bool reboot = false;
  bool has_cmd = parse_commands_from_body(body, "HB", &reboot);
  g_last_hb_malformed = g_last_cmd_parse_malformed;
  if (has_cmd) g_empty_hb_cycles = 0;
  g_last_cloud_ok_ms = HAL_GetTick();
  g_cloud_was_online = true;
  g_transport_fail_count = 0;
  ESP8266_ClearRecoveryFailureCount();
  LOG_I("TAPI", "Heartbeat OK%s", has_cmd ? ", ack pending" : "");
  return true;
}

static void start_heartbeat(void) {
  char body[256];
  if (!build_heartbeat_body(body, sizeof(body))) {
    LOG_W("TAPI", "Heartbeat body build failed");
    schedule_heartbeat(TOS_RETRY_MS);
    return;
  }

  if (strlen(body) > 220U) {
    LOG_W("TAPI", "Heartbeat body large: %u", (unsigned)strlen(body));
  }

  if (Net_AsyncHttpPostStart(TOS_API_HOST, TOS_API_PORT,
                             TOS_HEARTBEAT_PATH, body,
                             (uint16_t)strlen(body), 6500U)) {
    g_phase = TOS_PHASE_HEARTBEAT;
    LOG_D("TAPI", "Heartbeat queued, body=%uB expr=%s", (unsigned)strlen(body), EmotionManager_GetReportExpression());
  } else {
    note_transport_failure("heartbeat-start");
    schedule_heartbeat(TOS_RETRY_MS);
  }
}

static void start_ack_or_schedule(void) {
  if (!g_ack_body[0]) {
    /* Do not fallback-GET on every 3 s heartbeat.  POST heartbeat + GET every
     * cycle doubles AT traffic and can wedge/power-stress the ESP8266.  Heartbeat
     * is the primary command transport; fallback is for malformed responses or
     * a periodic safety check only. */
    bool should_poll = false;
    if (g_last_hb_malformed) {
      should_poll = true;
      g_last_hb_malformed = false;
    } else if (++g_empty_hb_cycles >= 60U) { /* low-rate safety poll */
      should_poll = true;
      g_empty_hb_cycles = 0;
    }

    if (should_poll && g_transport_fail_count == 0U) {
      (void)poll_commands_once();
    } else if (should_poll) {
      LOG_D("TAPI", "Skip fallback GET while transport is unstable");
    }
  }

  if (g_ack_body[0]) {
    if (Net_AsyncHttpPostStart(TOS_API_HOST, TOS_API_PORT, g_ack_path,
                               g_ack_body, (uint16_t)strlen(g_ack_body),
                               5000U)) {
      g_phase = TOS_PHASE_ACK;
      return;
    }
    LOG_W("TAPI", "Ack start failed");
    note_transport_failure("ack-start");
  }
  g_phase = TOS_PHASE_IDLE;
  schedule_heartbeat(TOS_HEARTBEAT_MS);
  LOG_D("TAPI", "Next heartbeat in %lums", (unsigned long)TOS_HEARTBEAT_MS);
}

void TosApi_Init(void) {
  build_device_id();
  g_phase = TOS_PHASE_IDLE;
  g_ack_body[0] = '\0';
  g_reboot_after_ack = false;
  g_initialized = true;
  g_last_cloud_ok_ms = 0;
  g_cloud_was_online = false;
  g_offline_for_this_boot = online_intended_config() && !network_ready();
  g_transport_fail_count = 0;
  g_last_rssi = wlan_enabled() ? -99 : 0;
  g_last_rssi_refresh_ms = 0;
  schedule_heartbeat(TOS_START_DELAY_MS);
  LOG_I("TAPI", "Cloud client init, di=%s hb=%lums mode=%s%s",
        g_device_id, (unsigned long)TOS_HEARTBEAT_MS,
        online_intended_config() ? "online-intended" : "offline",
        g_offline_for_this_boot ? "/offline-this-boot" : "");
}

static void note_transport_failure(const char *where) {
  uint32_t now = HAL_GetTick();
  if (g_transport_fail_count < 255U) g_transport_fail_count++;
  uint32_t offline_ms = g_last_cloud_ok_ms ? (now - g_last_cloud_ok_ms) : 0;
  LOG_W("TAPI", "%s transport fail #%u, offline=%lums",
        where ? where : "cloud", (unsigned)g_transport_fail_count,
        (unsigned long)offline_ms);

  if (!online_intended_config()) return;
  if (g_offline_for_this_boot) return;
  if (Net_IsHardDisabled()) return;

  /* If this boot has never reached cloud, treat the product as offline.
   * This covers WLAN disabled, auto-connect disabled, no saved AP, saved AP out
   * of range, or server unreachable before the first successful heartbeat. */
  if (!g_cloud_was_online || g_last_cloud_ok_ms == 0) return;

  if (g_transport_fail_count == 0U) return;
  if (offline_ms < TOS_NET_STUCK_MS) return;

  /* Once this boot has proven it was online, 30s without heartbeat/ack is a
   * whole-system network fault.  Do not attempt ESP recovery/re-init in runtime;
   * reboot through syshandle so the ESP returns to a clean startup path. */
  LOG_E("TAPI", "Cloud transport stuck for %lums; entering syshandle",
        (unsigned long)offline_ms);
  SysHandle_Exception(SYS_ERR_NET_TRANSPORT_STUCK);
}

void TosApi_Tick(void) {
  SysWatchdog_Tick();
  if (!g_initialized) return;

  Net_AsyncTick();
  NetAsyncState_t ns = Net_AsyncState();
  uint32_t now = HAL_GetTick();

  if (g_phase == TOS_PHASE_HEARTBEAT && ns != NET_ASYNC_BUSY) {
    bool ok = false;
    if (ns == NET_ASYNC_DONE) {
      ok = parse_heartbeat_response(Net_AsyncResponse());
    } else if (ns == NET_ASYNC_FAILED) {
      note_transport_failure("heartbeat");
    }

    Net_AsyncReset();
    if (ok) {
      start_ack_or_schedule();
    } else {
      g_phase = TOS_PHASE_IDLE;
      uint32_t retry = TOS_RETRY_MS;
      if (g_transport_fail_count > 0U) {
        retry += (uint32_t)g_transport_fail_count * 2000U;
        if (retry > 15000U) retry = 15000U;
      }
      schedule_heartbeat(retry);
    }
    return;
  }

  if (g_phase == TOS_PHASE_ACK && ns != NET_ASYNC_BUSY) {
    bool ack_ok = false;
    if (ns == NET_ASYNC_DONE) {
      int code = response_code(Net_AsyncResponse());
      ack_ok = (code == 0);
      if (ack_ok) {
        g_last_cloud_ok_ms = HAL_GetTick();
        g_cloud_was_online = true;
        g_transport_fail_count = 0;
        ESP8266_ClearRecoveryFailureCount();
      }
      LOG_I("TAPI", "Ack %s", ack_ok ? "OK" : "API fail");
    } else if (ns == NET_ASYNC_FAILED) {
      note_transport_failure("ack");
    }

    Net_AsyncReset();
    g_phase = TOS_PHASE_IDLE;

    if (ack_ok) {
      g_ack_body[0] = '\0';
      schedule_heartbeat(TOS_HEARTBEAT_MS);

      if (g_reboot_after_ack) {
        LOG_I("TAPI", "Reboot command acknowledged, resetting");
        HAL_Delay(30);
        NVIC_SystemReset();
      }
    } else {
      /* Commands are marked delivered by the backend once included in a
       * heartbeat response. If the ack POST fails and we drop g_ack_body, the
       * backend will not resend them and the console looks random/unreliable.
       * Keep the ack body and retry it before the next heartbeat.
       */
      LOG_W("TAPI", "Ack retained, retry soon");
      schedule_heartbeat(5000U);
    }
    return;
  }

  if (g_phase != TOS_PHASE_IDLE) return;
  if (ns == NET_ASYNC_DONE || ns == NET_ASYNC_FAILED) Net_AsyncReset();
  if (!time_due(now, g_next_heartbeat_ms)) {
    uint32_t until_hb = g_next_heartbeat_ms - now;
    if (until_hb > TOS_RSSI_IDLE_BUDGET_MS) maybe_refresh_rssi(now);
    return;
  }

  if (!wlan_enabled()) {
    /* User-selected offline mode: no cloud traffic and no system error. */
    g_cloud_was_online = false;
    g_last_cloud_ok_ms = 0;
    g_transport_fail_count = 0;
    schedule_heartbeat(10000U);
    return;
  }

  if (!network_ready()) {
    if (g_cloud_was_online) {
      note_transport_failure("wifi-offline");
    } else {
      /* WLAN is configured but not joined in this boot: out-of-range/offline
       * mode.  Do not re-init/recover ESP8266 and do not touch cloud. */
      g_offline_for_this_boot = online_intended_config();
      g_transport_fail_count = 0;
    }
    schedule_heartbeat(10000U);
    return;
  }

  if (g_ack_body[0]) {
    start_ack_or_schedule();
    return;
  }

  if (cloud_offline_mode()) {
    schedule_heartbeat(5000U);
    return;
  }

  start_heartbeat();
}

bool TosApi_IsBusy(void) {
  return g_phase != TOS_PHASE_IDLE || Net_AsyncState() == NET_ASYNC_BUSY;
}

bool TosApi_CheckUpgrade(TosUpgradeInfo *info) {
  if (!info) return false;

  if (Net_IsHardDisabled()) {
    LOG_W("TAPI", "ESP8266 is hard-disabled, cannot check upgrade");
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

  char resp[1024];
  memset(resp, 0, sizeof(resp));

  if (!Net_HttpGet(host, TOS_API_PORT, path, resp, sizeof(resp), 15000)) {
    LOG_E("TAPI", "HTTP request failed");
    return false;
  }

  LOG_D("TAPI", "Raw: %.200s", resp);

  const char *body = json_extract_body(resp);
  int code = json_get_int(body, "c", json_get_int(body, "code", -1));
  if (code != 0) {
    LOG_E("TAPI", "API error code: %d", code);
    return false;
  }

  info->has_update = json_get_bool(body, "has_update",
                                   json_get_bool(body, "hu", false));

  const char *latest = strstr(body, "\"latest\"");
  if (latest) {
    json_get_str(latest, "version", info->latest_version,
                 sizeof(info->latest_version));
    info->latest_version_code = json_get_int(latest, "version_code", 0);
    json_get_str(latest, "build", info->latest_build,
                 sizeof(info->latest_build));
    json_get_str(latest, "patch", info->latest_patch,
                 sizeof(info->latest_patch));
    json_get_str(latest, "url", info->latest_url, sizeof(info->latest_url));
    info->latest_size = json_get_int(latest, "size", 0);
    json_get_str(latest, "sha256", info->latest_sha256,
                 sizeof(info->latest_sha256));
  } else {
    json_get_str(body, "lv", info->latest_version,
                 sizeof(info->latest_version));
    info->latest_version_code = json_get_int(body, "lvc", 0);
    json_get_str(body, "lb", info->latest_build,
                 sizeof(info->latest_build));
    json_get_str(body, "lp", info->latest_patch,
                 sizeof(info->latest_patch));
    json_get_str(body, "lu", info->latest_url, sizeof(info->latest_url));
    info->latest_size = json_get_int(body, "ls", 0);
    json_get_str(body, "lh", info->latest_sha256,
                 sizeof(info->latest_sha256));
  }

  LOG_D("TAPI", "Latest: ver='%s' build='%s' patch='%s' size=%d",
        info->latest_version, info->latest_build, info->latest_patch,
        info->latest_size);
  LOG_I("TAPI", "Update check done. has_update=%d", info->has_update);
  return true;
}
