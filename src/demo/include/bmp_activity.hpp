#ifndef BMP_ACTIVITY_HPP
#define BMP_ACTIVITY_HPP

#ifdef __cplusplus
extern "C" {
#endif

void bmp180_test_activity(void);
void bmp180_continuous_activity(void);
void bmp180_altitude_activity(void);
void bmp180_debug_activity(void);
void bmp180_gui_activity(void); // 新增 GUI 版本

#ifdef __cplusplus
}
#endif

#endif // BMP_ACTIVITY_HPP