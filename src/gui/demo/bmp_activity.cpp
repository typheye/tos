#include "include/bmp_activity.hpp"
#include "hardware/include/bmp180.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/usart.hpp"
#include "include/libpd.h"
#include "include/libvan.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

extern USART boardSerial;
extern BMP180 boardBMP180;
extern KeyManager keyManager;
extern LCD boardLCD;

#define BMP_MENU_ITEMS 4
static const char *bmp_menus[BMP_MENU_ITEMS] = {
    "01 Real-time", "02 Chart", "03 Calibrate", "04 Back"};

static int menu_select = 0;
static float reference_pressure = 1013.25f;
static float altitude_offset = 0.0f;

#define CHART_HISTORY 120
static CCMRAM float chart_temp[CHART_HISTORY];
static CCMRAM float chart_press[CHART_HISTORY];
static CCMRAM int chart_idx = 0;
static CCMRAM float temp_max=50.0f,temp_min=-20.0f,press_max=1100.0f,press_min=900.0f;
static CCMRAM int chart_mode = 0;

// === 通用组件 ===
static void bar(const char *t) {
  PD_DrawFrame();
  PD_SetFont(FONT_ASCII_16); PD_SetColor(TOS_ACCENT);
  PD_DrawString(22,5,t);
}
static void bbar(const char *l,const char *m,const char *r) {
  PD_DrawFooterCenter(l,m,r);
}
static void menu_cards(int sel) {
  PD_SetFont(FONT_ASCII_16);
  for(int i=0;i<BMP_MENU_ITEMS;i++){
    int cy=33+i*25;
    if(i==sel){PD_DrawAngledCard(14,cy,212,20,5,TOS_ACCENT);PD_SetColor(TOS_TEXT);}
    else{PD_DrawAngledCard(14,cy,212,20,5,TOS_CARD_BG);PD_SetColor(TOS_TEXT_SEC);}
    PD_DrawString(26,cy+2,bmp_menus[i]);
  }
}

static void update_chart(float t,float p){
  chart_temp[chart_idx]=t; chart_press[chart_idx]=p; chart_idx++;
  if(chart_idx>=CHART_HISTORY)chart_idx=0;
  if (t > temp_max) temp_max = t + 2;
  if (t < temp_min) temp_min = t - 2;
  if (p > press_max) press_max = p + 20;
  if (p < press_min) press_min = p - 20;
  if(temp_max-temp_min>100){temp_max=temp_min+100;}
  if(press_max-press_min>500){press_max=press_min+500;}
}
static void rst_chart(){
  for(int i=0;i<CHART_HISTORY;i++){chart_temp[i]=25.0f;chart_press[i]=1013.25f;}
  chart_idx=0;temp_max=50;temp_min=-20;press_max=1100;press_min=900;
}

static void chart_axes(int x,int y,int w,int h,float mx,float mn,const char*u){
  PD_SetColor(LV_BORDER);PD_DrawRect(x,y,w,h);
  for(int i=1;i<=3;i++){int ly=y+(h*i/4);PD_SetColor(LV_BORDER);PD_DrawLine(x,ly,x+w,ly);}
  PD_SetFont(FONT_ASCII_12);PD_SetColor(LV_TEXT_HINT);
  char lb[16];snprintf(lb,sizeof(lb),"%.0f%s",mx,u);PD_DrawString(x-28,y-4,lb);
  snprintf(lb,sizeof(lb),"%.0f%s",mn,u);PD_DrawString(x-28,y+h-4,lb);
}
static void chart_line(float*d,int x,int y,int w,int h,float mx,float mn,uint32_t c){
  PD_SetColor(c);float r=mx-mn;if(r<0.01f)r=1.0f;
  for(int i=1;i<w&&i<CHART_HISTORY;i++){
    int pv=(chart_idx-1-i+CHART_HISTORY)%CHART_HISTORY;
    int cv=(chart_idx-i+CHART_HISTORY)%CHART_HISTORY;
    int y1=y+h-(int)((d[pv]-mn)*h/r);int y2=y+h-(int)((d[cv]-mn)*h/r);
    if (y1 < y)       y1 = y;
    if (y1 > y + h)   y1 = y + h;
    if (y2 < y)       y2 = y;
    if (y2 > y + h)   y2 = y + h;
    PD_DrawLine(x+w-i,y1,x+w-(i-1),y2);
  }
}

// === 实时显示 ===
void bmp180_display_activity(void){
  if(!boardBMP180.isInitialized()){boardBMP180.init();}
  if(!boardBMP180.isInitialized()){
    LCD_FLUSH({
      PD_FillScreen(LV_BG_DARK);
      PD_SetFont(FONT_ASCII_16);PD_SetColor(LV_ERROR);
      PD_DrawString(20,80,"BMP180 Init Failed!");
    });HAL_Delay(2000);return;
  }
  PD_FillScreen(LV_BG_DARK);
  uint32_t lu=HAL_GetTick();
  while(1){
    keyManager.btn_enter.tick();
    if(keyManager.btn_enter.getState()==KEY_PRESSED)break;
    if(HAL_GetTick()-lu>200){lu=HAL_GetTick();
      BMP180_Data_t d=boardBMP180.readData(BMP180_MODE_STD);
      float alt=boardBMP180.calcAltitude(d.pressure,reference_pressure)-altitude_offset;
      LCD_FLUSH({
        PD_FillScreen(LV_BG_DARK);bar("05");

        PD_DrawAngledCard(8,44,224,100,6,TOS_CARD_BG);
        char f[16],db[64];PD_SetFont(FONT_ASCII_16);
        PD_SetColor(TOS_GREY);PD_DrawString(16,54,"Temperature");
        float_to_str(d.temperature,f);sprintf(db,"%s C",f);
        PD_SetColor(TOS_TEXT);PD_DrawString(152,54,db);

        PD_SetColor(TOS_GREY);PD_DrawString(16,76,"Pressure");
        float_to_str(d.pressure,f);sprintf(db,"%s hPa",f);
        PD_SetColor(TOS_TEXT);PD_DrawString(152,76,db);

        PD_SetColor(TOS_GREY);PD_DrawString(16,98,"Altitude");
        float_to_str(alt,f);sprintf(db,"%s m",f);
        PD_SetColor(TOS_ACCENT);PD_DrawString(152,98,db);

        PD_SetFont(FONT_ASCII_12);PD_SetColor(TOS_GREY);
        float_to_str(reference_pressure,f);sprintf(db,"Ref: %s hPa",f);
        PD_DrawString(16,126,db);

        PD_DrawAngledCard(8,152,224,56,6,TOS_CARD_BG);
        PD_SetFont(FONT_ASCII_16);
        PD_SetColor(TOS_GREY);PD_DrawString(16,162,"Ref. Pressure");
        float_to_str(reference_pressure,f);sprintf(db,"%s hPa",f);
        PD_SetColor(TOS_TEXT);PD_DrawString(140,162,db);

        bbar("EXIT",NULL,NULL);
      });
    }
    HAL_Delay(50);
  }
}

// === 图表模式 ===
void bmp180_chart_activity(void){
  uint32_t lu=HAL_GetTick();uint8_t la8=0,ld0=0;uint32_t lsw=0;
  rst_chart();chart_mode=0;
  if(!boardBMP180.isInitialized()){boardBMP180.init();if(!boardBMP180.isInitialized())return;}
  PD_FillScreen(LV_BG_DARK);
  while(1){
    keyManager.btn_enter.tick();if(keyManager.btn_enter.getState()==KEY_PRESSED)break;
    keyManager.collision_A8.tick();keyManager.collision_D0.tick();
    uint8_t ca=(keyManager.collision_A8.getState()==KEY_PRESSED);
    if(ca&&!la8&&(HAL_GetTick()-lsw>300)){chart_mode=!chart_mode;lsw=HAL_GetTick();}la8=ca;
    uint8_t cd=(keyManager.collision_D0.getState()==KEY_PRESSED);
    if (cd && !ld0) rst_chart();
    ld0 = cd;
    BMP180_Data_t d=boardBMP180.readData(BMP180_MODE_STD);
    update_chart(d.temperature,d.pressure);
    if(HAL_GetTick()-lu>50){lu=HAL_GetTick();
      LCD_FLUSH({
        PD_FillScreen(LV_BG_DARK);
        char t[32];snprintf(t,sizeof(t),chart_mode?"Pressure":"Temperature");bar(t);
        int cx=10,cy=44,cw=220,ch=100;
        if(chart_mode==0){chart_axes(cx,cy,cw,ch,temp_max,temp_min,"C");chart_line(chart_temp,cx,cy,cw,ch,temp_max,temp_min,LV_WARNING);}
        else{chart_axes(cx,cy,cw,ch,press_max,press_min,"hPa");chart_line(chart_press,cx,cy,cw,ch,press_max,press_min,LV_ACCENT);}
        PD_SetFont(FONT_ASCII_16);PD_SetColor(LV_TEXT_PRIMARY);
        char db[64],f[16];
        float_to_str(chart_mode?d.pressure:d.temperature,f);
        snprintf(db,sizeof(db),"%s %s",f,chart_mode?"hPa":"C");
        PD_DrawString(14,152,db);
        PD_SetFont(FONT_ASCII_12);PD_SetColor(LV_TEXT_HINT);
        snprintf(db,sizeof(db),"Range:%.0f-%.0f",chart_mode?press_min:temp_min,chart_mode?press_max:temp_max);
        PD_DrawString(14,172,db);
        PD_SetColor(LV_PRIMARY);PD_SetFill(true);
        PD_DrawRoundRect(14,190,chart_mode?100:100,6,3);PD_SetFill(false);
        bbar("EXIT",NULL,"UP/DOWN");
      });
    }HAL_Delay(30);
  }
}

// === 校准 ===
void bmp180_calibrate_activity(void){
  int st=0;uint32_t lu=HAL_GetTick();uint8_t le=0;float nr=reference_pressure;
  if(!boardBMP180.isInitialized()){boardBMP180.init();if(!boardBMP180.isInitialized())return;}
  PD_FillScreen(LV_BG_DARK);
  while(1){
    keyManager.btn_enter.tick();keyManager.collision_D0.tick();
    uint8_t ce=(keyManager.btn_enter.getState()==KEY_PRESSED);
    if(keyManager.collision_D0.getState()==KEY_PRESSED&&st==0)break;
    if(ce&&!le){if(st==0){BMP180_Data_t d=boardBMP180.readData(BMP180_MODE_STD);nr=d.pressure;st=1;}
    else if(st==1){reference_pressure=nr;break;}}le=ce;
    if(HAL_GetTick()-lu>100){lu=HAL_GetTick();
      BMP180_Data_t d=boardBMP180.readData(BMP180_MODE_STD);
      float ca=boardBMP180.calcAltitude(d.pressure,nr);
      LCD_FLUSH({
        PD_FillScreen(LV_BG_DARK);bar("Calibrate");
        PD_SetFont(FONT_ASCII_16);
        PD_DrawAngledCard(8,44,224,70,6,TOS_CARD_BG);
        if(st==0){PD_SetColor(LV_WARNING);PD_DrawString(16,54,"Set MIN (rotate CCW)");}
        else{PD_SetColor(LV_SUCCESS);PD_DrawString(16,54,"Set MAX (rotate CW)");}
        char db[64],f[16];PD_SetColor(LV_TEXT_PRIMARY);
        float_to_str(d.pressure,f);snprintf(db,sizeof(db),"Current: %s hPa",f);
        PD_DrawString(16,78,db);
        PD_DrawAngledCard(8,122,224,56,6,TOS_CARD_BG);
        PD_SetColor(LV_ACCENT);
        if(st==0)PD_DrawString(16,132,"Enter -> Set Reference");
        else PD_DrawString(16,132,"Enter -> Save");
        float_to_str(ca,f);
        PD_SetFont(FONT_ASCII_12);PD_SetColor(LV_TEXT_HINT);
        snprintf(db,sizeof(db),"Altitude: %s m",f);
        PD_DrawString(16,155,db);
        bbar("EXIT",NULL,NULL);
      });
    }HAL_Delay(50);
  }
}

// === 主菜单 ===
void bmp180_activity(void){
  uint8_t le=0;uint32_t lu=HAL_GetTick();
  PD_Init();menu_select=0;boardBMP180.init();PD_FillScreen(LV_BG_DARK);
  while(1){
    keyManager.collision_A8.tick();keyManager.collision_D0.tick();keyManager.btn_enter.tick();
    if(keyManager.collision_A8.getState()==KEY_PRESSED){menu_select++;if(menu_select>=BMP_MENU_ITEMS)menu_select=BMP_MENU_ITEMS-1;HAL_Delay(150);}
    if(keyManager.collision_D0.getState()==KEY_PRESSED){if(menu_select>0)menu_select--;HAL_Delay(150);}
    uint8_t ce=(keyManager.btn_enter.getState()==KEY_PRESSED);
    if(ce&&!le){switch(menu_select){case 0:bmp180_display_activity();break;case 1:bmp180_chart_activity();break;case 2:bmp180_calibrate_activity();break;case 3:return;}PD_FillScreen(LV_BG_DARK);}le=ce;
    if(HAL_GetTick()-lu>100){lu=HAL_GetTick();LCD_FLUSH({
      PD_FillScreen(LV_BG_DARK);bar("05");
      PD_SetFont(FONT_ASCII_12);PD_SetColor(LV_TEXT_HINT);PD_DrawString(16,28,"Select Function:");
      menu_cards(menu_select);bbar("ENTER",NULL,"UP/DOWN");
    });
    }HAL_Delay(1);
  }
}