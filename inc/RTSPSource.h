#ifndef RTSP_SOURCE_H_
#define RTSP_SOURCE_H_


#include <string>

#include "api/peerconnectioninterface.h"
#include "api/test/fakeconstraints.h"

#include "modules/audio_device/include/audio_device.h"

#include "rtc_base/logging.h"
#include "rtc_base/json.h"
#include "RTSPSource.h"


		// represents a RTSP URL that can be requested by peers.
		// id = unique id
		// url = rtsp:// url
		// transport
		// other options...
		class RTSPSource : public rtc::RefCountInterface
		{

			public:
				RTSPSource(std::string id, std::string url, std::string transport, std::string name)
					: id(id), url(url), transport(transport), name(name)
				{
				}

			const std::string getID() { return id; }	// unique id for stream
			const std::string getURL() { return url; }	// rtsp url for stream
			const std::string getTransport() { return transport; }	// udp, tcp,
			const std::string getName() { return name; }	// (unique) name for stream

			const std::string toString() {
					std::string out = getID();
					return out;
			 };	// unique id for stream

			 int getTimeout()
			 {
         if (timeout<=0)
          timeout=30;
				 return timeout;
			 }
      int getFPS() { return fps;}

			 // bool isBaseline() { return false; }	// TODO: return true if baseline h264
       void setTimeout(int i)
       {
         timeout = i;
       }

			protected:

        const std::string id;
        const std::string url;
				const std::string transport;
				const std::string name;
				bool already_baseline = false;		// true if we can skip the decode/encode
				int fps=25;	// float?
				int timeout = 30;

			// the destructor may be called from any thread.
			~RTSPSource() {
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << " ~RTSPSource:" << getID();
			}
		};



#endif
