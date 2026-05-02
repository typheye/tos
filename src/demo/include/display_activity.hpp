#ifndef DISPLAY_ACTIVITY_HPP
#define DISPLAY_ACTIVITY_HPP

#ifdef __cplusplus
extern "C" {
#endif

// 统一的显示测试菜单函数
void display_test_menu_activity(void);

// 保留原有函数供直接调用
void display_test_activity(void);
void display_clear_activity(void);
void display_color_bars_activity(void);
void display_pattern_activity(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_ACTIVITY_HPP