#ifndef __SN74HC00N_ACTIVITY_HPP
#define __SN74HC00N_ACTIVITY_HPP

#ifdef __cplusplus
extern "C" {
#endif

// SN74HC00N GUI 主菜单
void hc00n_activity(void);

// 实时监测模式（表格显示）
void hc00n_monitor_activity(void);

// 真值表演示
void hc00n_truth_table_activity(void);

// 逻辑测试模式
void hc00n_test_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* __SN74HC00N_ACTIVITY_HPP */