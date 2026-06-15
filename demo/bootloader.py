# -*- coding:utf-8 -*-
# LCD显示控制

import time
import RPi.GPIO as GPIO
from system.power import PowerManager
import spidev as SPI  # 添加SPI导入
from PIL import Image, ImageDraw
from hardware.st7789 import ST7789
from config import *
from display.graphics import Graphics
from display.fonts import fonts
from utils import press_key, wait_release, wait_press, log


class Bootloader:
    PRODUCT_NAME = "tcarone"
    VERSION = ""
    VERSION_BOOTLOADER = "TCARONEV1.0"
    VERSION_BASEBAND = "N/A"
    UNLOCKED = "no"
    def __init__(self, device):
        self.device = device
        self.canvas = None
        self.current_image = None
        self.menus = ["Reboot", "Reboot to recovery", "Reboot to bootloader"]
        self.menus_index = 0
        self.flag = "BOOTLOADER"

    def create_canvas(self):
        """创建画布上下文管理器"""

        class Canvas:
            def __init__(self, lcd_controller):
                self.lcd = lcd_controller
                self.image = Image.new(
                    "RGB", (SCREEN_WIDTH, SCREEN_HEIGHT), COLOR_BLACK
                )
                self.draw = ImageDraw.Draw(self.image)

            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc_val, exc_tb):
                self.lcd.device.ShowImage(self.image, 0, 0)
                self.lcd.current_image = self.image
                pass

        return Canvas(self)

    def clear(self):
        """清屏"""
        self.device.clear()

    def show_image(self, image, x=0, y=0):
        """显示图像"""
        self.device.ShowImage(image, x, y)

    def show_image_fast(self, image, cam_fps=0, disp_fps=0):
        """显示图像"""
        self.device.ShowImageFast(image, cam_fps=cam_fps, disp_fps=disp_fps)

    def show_main(self):
        """显示BOOTLOADER页"""
        with self.create_canvas() as canvas:
            canvas.draw.rectangle((0, 0, SCREEN_WIDTH, SCREEN_HEIGHT), fill=COLOR_BLACK)
            text = "FASTBOOT MENU"
            w, h = canvas.draw.textsize(text, font=fonts.get_font("normal"))
            canvas.draw.text(
                ((SCREEN_WIDTH - w) // 2, 15),
                text,
                font=fonts.get_font("normal"),
                fill=COLOR_WHITE,
            )

            text = "select:\nshort press the button\ncontinue:\nshort press the button"
            canvas.draw.text(
                (15, h + 30), text, font=fonts.get_font("small"), fill=COLOR_WHITE
            )

            menu = self.menus[self.menus_index]
            canvas.draw.text(
                (15, h + 100), menu, font=fonts.get_font("small"), fill=COLOR_RED
            )

            info = f"product_name:{Bootloader.PRODUCT_NAME}\nversion:{Bootloader.VERSION}\nversion-bootloader:{Bootloader.VERSION_BOOTLOADER}\nversion-baseband:{Bootloader.VERSION_BASEBAND}\nunlocked:{Bootloader.UNLOCKED}"
            canvas.draw.text(
                (15, h + 130), info, font=fonts.get_font("small"), fill=COLOR_WHITE
            )

            # 处理按键
            if press_key(main_PIN):
                press_time = wait_release(main_PIN)
                if press_time > 800:
                    self.key_press()
                else:
                    self.switch_menu()

    def switch_menu(self):
        if self.menus_index < len(self.menus) - 1:
            self.menus_index += 1
        else:
            self.menus_index = 0

    def key_press(self):
        if self.menus_index == 0:
            self.clear()
            log(3, "系统重启中...")
            time.sleep(2)
            PowerManager.reboot()
        elif self.menus_index == 1:
            self.set_flag("RECOVERY")
            self.clear()
            time.sleep(1)
        elif self.menus_index == 2:
            self.menus_index = 0
            self.clear()
            time.sleep(1)

    def get_flag(self):
        return self.flag
    
    def set_flag(self, flag):
        self.flag = flag
