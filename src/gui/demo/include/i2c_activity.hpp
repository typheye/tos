#ifndef I2C_ACTIVITY_HPP
#define I2C_ACTIVITY_HPP

#ifdef __cplusplus
extern "C" {
#endif

void i2c_scan_activity(void);
void i2c_scan_activity_gui(void); // 新增 GUI 版本

#ifdef __cplusplus
}
#endif

#endif // I2C_ACTIVITY_HPP