#ifndef SD_ACTIVITY_HPP
#define SD_ACTIVITY_HPP

#ifdef __cplusplus
extern "C" {
#endif

// 主 GUI 界面
void sd_card_activity_gui(void);
void sd_card_activity(void);        // 兼容旧接口
void sd_card_direct_activity(void); // 硬件直接测试

// 诊断和测试
void sd_card_diagnostic(
    void); // 完整诊断（合并 debug_test + real_capacity + quick_check）
void sd_card_rw_test(void); // 读写测试（合并 simple_test + fatfs_test）

// 文件系统操作
void sd_card_mount(void);   // 挂载并显示信息
void sd_card_unmount(void); // 卸载
void sd_card_list(void);    // 列出文件
void sd_card_format(void);  // 格式化（谨慎使用）

#ifdef __cplusplus
}
#endif

#endif // SD_ACTIVITY_HPP