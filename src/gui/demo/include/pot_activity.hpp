#ifndef __POT_ACTIVITY_HPP
#define __POT_ACTIVITY_HPP

#ifdef __cplusplus
extern "C" {
#endif

// 主菜单
void pot_activity(void);

// 实时显示模式
void pot_monitor_activity(void);

// 图表显示模式 (线性图表)
void pot_chart_activity(void);

// 校准模式
void pot_calibrate_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* __POT_ACTIVITY_HPP */