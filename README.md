# RemotePasteboard

Share your paste board with other devices by net.

## 背景
在一次配置archlinux时，当时还没有配好hyprland,一个一个敲真的很麻烦，从其他地方复制粘贴也不方便，于是我就想能不能和我的另一台电脑共享粘贴板，于是便有了这个项目

## 通过服务器的tcp共享粘贴板

目前支持linux（wayland&wl-clipboard）和windows平台，是需要用户部署在自己的服务器的

## 使用方法

### Windows:

- 可以使用QtCreater构建

- 可以使用Cmake构建，由于这个项目使用的是MinGW,你需要执行
  
  ```bash
  mkdir build
  cd build
  cmake .. -G "MinGW Makefile"
  make
  ```
  
  ### Linux
  
  ```bash
  mkdir build
  cd build
  cmake ..
  make ..
  ```
  
  ### 服务器端
  
  ```bash
  # Ubuntu/Debian
  sudo apt-get install nlohmann-json3-dev
  
  # CentOS/RHEL
  sudo yum install nlohmann-json-devel
  
  # Arch Linux
  sudo pacman -S nlohmann-json
  
  
  --------------
  mkdir build
  cd build
  cmake ..
  make
  
  
  ./可执行文件 -p port [-u user -w password]
  ```
  
  
  
  
