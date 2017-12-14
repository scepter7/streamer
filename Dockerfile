FROM heroku/heroku:16
LABEL maintainer michel.promonet@free.fr

WORKDIR /streamer
ADD . /streamer

# Get tools for WebRTC
RUN git clone --depth 1 https://chromium.googlesource.com/chromium/tools/depot_tools.git
ENV PATH /streamer/depot_tools:$PATH
ENV SYSROOT /webrtc/src/build/linux/debian_stretch_amd64-sysroot


# Build
RUN apt-get update && apt-get install -y --no-install-recommends g++ autoconf automake libtool xz-utils libasound-dev \
        && mkdir /webrtc \
	&& cd /webrtc \
	&& fetch --no-history --nohooks webrtc \
	&& sed -i -e "s|'src/resources'],|'src/resources'],'condition':'rtc_include_tests==true',|" src/DEPS \
	&& gclient sync \
	&& make -C /streamer live555 \
	&& cd src \
	&& sed -i -e 's|"examples",||' BUILD.gn \
	&& gn gen out/Release --args='is_debug=false rtc_use_h264=true ffmpeg_branding="Chrome" rtc_include_tests=false rtc_enable_protobuf=false use_custom_libcxx=false use_ozone=true rtc_include_pulse_audio=false' \
	&& ninja -C out/Release jsoncpp rtc_json webrtc \
	&& cd /streamer \
	&& make all \

# Make port 8000 available to the world outside this container
EXPOSE 8000

# Run when the container launches
ENTRYPOINT [ "./streamer" ]

