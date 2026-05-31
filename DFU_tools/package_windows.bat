@echo off
setlocal
cd /d %~dp0
py -m pip install -r requirements.txt pyinstaller
py -m PyInstaller --noconfirm --clean --windowed --name TOS_DFU_Upgrade --add-data "config.json;." --add-data "bin;bin" run_gui.py

echo.
echo 打包完成：DFU_tools\dist\TOS_DFU_Upgrade\TOS_DFU_Upgrade.exe
echo 如需使用 dfu-util 后端，请把 dfu-util.exe 放到 dist\TOS_DFU_Upgrade\bin\
pause
