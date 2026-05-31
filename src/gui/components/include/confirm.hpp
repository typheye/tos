#ifndef CONFIRM_HPP
#define CONFIRM_HPP

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Show a Yes/No confirmation dialog
 * @param  title  Dialog title
 * @param  msg    Message text
 * @return true if Yes selected, false if No selected
 */
bool confirm_show(const char *title, const char *msg);

#ifdef __cplusplus
}
#endif

#endif
