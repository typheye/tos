#ifndef __BMP_ACTIVITY_HPP
#define __BMP_ACTIVITY_HPP

#ifdef __cplusplus
extern "C" {
#endif

// BMP180 GUI 主菜单
void bmp180_activity(void);

// 实时显示模式（温度/气压/海拔）
void bmp180_display_activity(void);

// 图表模式（气压/温度曲线）
void bmp180_chart_activity(void);

// 校准设置
void bmp180_calibrate_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* __BMP_ACTIVITY_HPP */