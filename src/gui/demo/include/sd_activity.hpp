/**
 ******************************************************************************
 * @file    sd_activity.hpp
 * @author  Typheye
 * @brief   Sd Activity interface.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021-2026 Typheye. All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#ifndef SD_ACTIVITY_HPP
#define SD_ACTIVITY_HPP

#ifdef __cplusplus
extern "C" {
#endif


void sd_card_activity_gui(void);
void sd_card_activity(void);        
void sd_card_direct_activity(void); 


void sd_card_diagnostic(
    void); 
void sd_card_rw_test(void); 


void sd_card_mount(void);   
void sd_card_unmount(void); 
void sd_card_list(void);    
void sd_card_format(void);  

#ifdef __cplusplus
}
#endif

#endif // SD_ACTIVITY_HPP