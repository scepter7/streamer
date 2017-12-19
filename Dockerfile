FROM heroku/heroku:16


# Get tools for WebRTC

ENV SYSROOT /webrtc/src/build/linux/debian_stretch_amd64-sysroot
RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git /usr/local/sbin
WORKDIR /streamer
RUN pwd

# Build
RUN apt-get update \
	&& apt-get install -y --no-install-recommends g++ autoconf automake libtool xz-utils libasound-dev \
	&& mkdir -p /webrtc && cd /webrtc \
	&& fetch --nohooks webrtc \
	&& gclient sync \
	&& cd src \
	&& gn gen out/Release --args='is_debug=false rtc_use_h264=true ffmpeg_branding="Chrome" rtc_include_tests=false rtc_enable_protobuf=false use_custom_libcxx=false use_ozone=true rtc_include_pulse_audio=false rtc_build_examples=false' \
	&& ninja -C out/Release jsoncpp rtc_json webrtc


ADD . /streamer
RUN cd /streamer &&  make live555 && make all
VOLUME /webrtc
VOLUME /streamer

# Make port 8000 available to the world outside this container
EXPOSE 8000

# Run when the container launches
ENTRYPOINT [ "./streamer" ]

