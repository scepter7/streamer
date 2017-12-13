streamer
===============
H.264 RTSP to WebRTC server based on a fork of [WebRTC-streamer](https://github.com/mpromonet/webrtc-streamer). In progress!


TODO tasks
===============
Add -k to streamer command line arguments to set adminKey  
Remove rtsp url from SDP
Are we using the turn server? Currently set to turn:none:none@bhlowe.com -- Brad will turn off turn server and test.


Added API calls to:
  Manage tokens for security access to streams
  Add/Delete h.264 RTSP streams


It embeds a HTTP server that implements API and serve a simple HTML page that use them through AJAX.   

The WebRTC signaling is implemented throught HTTP requests:

All calls are prefixed with /webrtc-api

 - /call   : send offer and get answer
 - /hangup : close a call
 - /addIceCandidate : add a candidate
 - /getIceCandidate : get the list of candidates
 - /listStreams : list rtsp url/name
 - /addStream : add an rtsp stream
 - /removeStream : remove an rtsp stream
 - /addToken : add a token to authorize using a stream
 - /removeToken : revoke a token


The list of HTTP API is available using /help.


Dependencies :
-------------
It is based on :
 * [WebRTC Native Code Package](http://www.webrtc.org)
 * [civetweb HTTP server](https://github.com/civetweb/civetweb)
 * [h264bitstream](https://github.com/aizvorski/h264bitstream)
 * [live555](http://www.live555.com/liveMedia)

Build
===============

Build WebRTC with H264 support
-------
	mkdir ../webrtc
	pushd ../webrtc
	fetch webrtc
	gn gen out/Release --args='is_debug=false use_custom_libcxx=false rtc_use_h264=true ffmpeg_branding="Chrome" rtc_include_tests=false use_ozone=true rtc_include_pulse_audio=false'
	ninja -C out/Release
	popd


Build live555 to enable RTSP support(optional)
-------
	make SYSROOT=<path to WebRTC>/src/build/linux/debian_stretch_amd64-sysroot live555

Build Streamer
-------
	make WEBRTCROOT=<path to WebRTC> WEBRTCBUILD=<Release or Debug> SYSROOT=<path to WebRTC>/src/build/linux/debian_stretch_amd64-sysroot

where WEBRTCROOT and WEBRTCBUILD indicate how to point to WebRTC :
 - $WEBRTCROOT/src should contains source (default is ../webrtc)
 - $WEBRTCROOT/src/out/$WEBRTCBUILD should contains libraries (default is Release)
 - $SYSROOT should point to sysroot used to build WebRTC (default is /)

Multiple Builds
-------
  * for x86_64 on Ubuntu Xenial
  * for armv7 crosscompiling with gcc-linaro-arm-linux-gnueabihf-raspbian-x64 (this build is running on Raspberry Pi2 and NanoPi NEO)
  * for armv6+vfp crosscompiling with gcc-linaro-arm-linux-gnueabihf-raspbian-x64 (this build is running on Raspberry PiB and should run on a Raspberry Zero)


Usage
===============
	./webrtc-streamer [-H http port] [-S[embeded stun address]] -[v[v]]  [url1]...[urln]
	./webrtc-streamer [-H http port] [-s[external stun address]] -[v[v]] [url1]...[urln]
	./webrtc-streamer -V
         	-H [hostname:]port : HTTP server binding (default 0.0.0.0:8000)
         	-S[stun_address]   : start embeded STUN server bind to address (default 0.0.0.0:3478)
         	-s[stun_address]   : use an external STUN server (default stun.l.google.com:19302)
                -t[username:password@]turn_address : use an external TURN relay server (default disabled)		
                -a[audio layer]    : specify audio capture layer to use (default:3)		
         	[url]              : url to register in the source list
        	-v[v[v]]           : verbosity
        	-V                 : print version


Example
-----

[Live Demo](https://test.siteproxy.net/)
[SiteProxy Demo](https://dev.siteproxy.net/)


Docker image
===============
Build: (Takes a long time! Has been known to fail.)
  build -t streamer .

You can start the application using the docker image :

 docker run -p 8000:8000 -it streamer

The container accept arguments that are forward to webrtc-streamer application, then you can :
