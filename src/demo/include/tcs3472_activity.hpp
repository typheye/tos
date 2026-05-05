#ifndef __TCS3472_ACTIVITY_HPP
#define __TCS3472_ACTIVITY_HPP

#ifdef __cplusplus
extern "C" {
#endif

// TCS3472 GUI 主菜单
void tcs3472_activity(void);

// 颜色显示测试
void tcs3472_color_demo(void);

// 色温检测
void tcs3472_cct_demo(void);

// 简单读数
void tcs3472_read_activity(void);

// 图表模式 (新增)
void tcs3472_chart_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* __TCS3472_ACTIVITY_HPP */