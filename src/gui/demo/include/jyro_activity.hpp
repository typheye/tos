#ifndef __JYRO_ACTIVITY_HPP
#define __JYRO_ACTIVITY_HPP

#ifdef __cplusplus
extern "C" {
#endif

// JY901S GUI 主菜单
void jyro_activity(void);

// 3D 立方体显示（原 gyro_cube_activity）
void jyro_cube_activity(void);

// 文本数据显示（所有参数）
void jyro_text_activity(void);

// 图表显示模式（可切换参数）
void jyro_chart_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* __JYRO_ACTIVITY_HPP */