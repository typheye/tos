/**
 * @file    tos_api.h
 * @brief   Cloud API client for TOS (upgrade check, etc.)
 */

#ifndef TOS_API_H
#define TOS_API_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOS_API_HOST "proxy-api.otodone.com"
#define TOS_API_PORT 80
#define TOS_API_PATH "/tos/upgrade/check"
#define TOS_API_V1_BASE "/v1"

typedef struct {
  bool has_update;
  char latest_version[8];
  int  latest_version_code;
  char latest_build[16];
  char latest_patch[16];
  char latest_url[128];
  int  latest_size;
  char latest_sha256[72];
} TosUpgradeInfo;

/**
 * @brief  Check for firmware update from cloud API
 * @param  info  Output struct (only valid if returns true)
 * @return true on success, false on network/parse error
 */
bool TosApi_CheckUpgrade(TosUpgradeInfo *info);

/**
 * @brief  Initialize background cloud heartbeat / command handling.
 */
void TosApi_Init(void);

/**
 * @brief  Advance background cloud communication.
 *         Call frequently from UI/main loops. Never blocks on network I/O.
 */
void TosApi_Tick(void);

/**
 * @brief  Whether the background cloud client owns the ESP8266 now.
 */
bool TosApi_IsBusy(void);

#ifdef __cplusplus
}
#endif

#endif
