# CPP 2D Game Engine

sudo apt-get install libsdl2-dev liblua5.4-dev
# 安装开发依赖（Ubuntu/Debian）
sudo apt-get install build-essential
sudo apt-get install libasound2-dev libpulse-dev libaudio-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev libxxf86vm-dev libxss-dev libgl1-mesa-dev libesd0-dev libdbus-1-dev libudev-dev libibus-1.0-dev fcitx-libs-dev libsamplerate0-dev libsndio-dev

glad.dav1d.de

git submodule add https://github.com/nothings/stb.git include/stb
~git submodule add https://github.com/richgel999/miniz.git include/miniz~
using the release version of miniz instead and copy to src

# 初始化并更新子模块
git submodule update --init --recursive