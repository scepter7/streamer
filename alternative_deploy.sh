#!/bin/bash 
 
if [ ! -d "depot_tools" ]; then 
  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git 
fi 
 
sudo apt-get update 
sudo apt-get install -y --no-install-recommends g++ autoconf automake libtool xz-utils libasound-dev 
 
export PATH=`pwd`/depot_tools:$PATH 
 
if [ ! -d "webrtc" ]; then 
  mkdir webrtc 
  cd webrtc 
  fetch --no-history --nohooks webrtc 
  gclient sync 
  cp src/build/install-build-deps.sh ./ 
  version=`lsb_release -r | grep -o '[0-9]*'` 
  arr=($version) 
  if [ ${arr[0]} == "14" -a ${arr[1]} == "04" ]; then 
    ./install-build-deps.sh --no-syms --no-arm --no-chromeos-fonts --no-nacl --no-promt 
  fi 
  if [ ${arr[0]} == "16" -a ${arr[1]} == "04" ]; then 
    ./install-build-deps.sh --no-syms --no-arm --no-chromeos-fonts --no-nacl --no-promt 
  fi 
  if [ ${arr[0]} == "17" -a ${arr[1]} == "04" ]; then 
    ./install-build-deps.sh --no-syms --no-arm --no-chromeos-fonts --no-nacl --no-promt 
  fi 
  if [ ${arr[0]} == "17" -a ${arr[1]} == "10" ]; then 
    ./install-build-deps.sh --no-syms --no-arm --no-chromeos-fonts --no-nacl --no-promt 
  fi 
  cd src 
  gn gen out/Debug --args='rtc_use_h264=true ffmpeg_branding="Chrome" rtc_include_tests=false rtc_enable_protobuf=false use_custom_libcxx=false use_ozone=true rtc_include_pulse_audio=false rtc_build_examples=false' 
  ninja -C out/Debug jsoncpp rtc_json webrtc 
fi 
