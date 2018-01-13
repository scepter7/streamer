/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** main.cpp
**
** -------------------------------------------------------------------------*/

#include <iostream>

#include "rtc_base/ssladapter.h"
#include "rtc_base/thread.h"
#include "p2p/base/stunserver.h"

#include "PeerConnectionManager.h"
#include "HttpServerRequestHandler.h"
static void split(std::string &f)
{
std::string::size_type ch = f.rfind('#');
std::string u=f;
std::string typ = "";

if (ch != std::string::npos)
{
	typ = f.substr(ch+1);
	u = f.substr(0, ch);
}
std::cout << "u= "<< u << " typ="<<typ << std::endl;

}


static void test()
{

std::string t1 = "rtsp://foobar:554/blah1#bang";
std::string t2 = "rtsp://foobar:554/blah2#";
std::string t3 = "rtsp://foobar:554/blah3";
std::string t4 = "#foobar";
split(t1);
split(t2);
split(t3);
split(t4);



}


/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[])
{
	const char* turnurl       = "";	//
	// const char* defaultlocalstunurl  = "0.0.0.0:3478";
	const char* localstunurl  = NULL;
	const char* stunurl       = "stun.l.google.com:19302";
	int logLevel              = rtc::INFO;	// bhl rtc::LERROR, LS_VERBOSE;


	webrtc::AudioDeviceModule::AudioLayer audioLayer = webrtc::AudioDeviceModule::kDummyAudio;
	// std::string apiPrefix="streamer_api/";
	if (false) { test(); return 0; }

	std::map<std::string,std::string> urlList;

	std::string httpAddress("0.0.0.0:");
	std::string httpPort = "8000";
	std::string authKey = "";
	bool test=false;

	const char * port = getenv("PORT");
	if (port)
	{
		httpPort = port;
	}
	httpAddress.append(httpPort);

	int c = 0;
	while ((c = getopt (argc, argv, "thVv:k:" "c:H:w:" "t:S::s::" "a::n:u:")) != -1)
	{
		switch (c)
		{
			case 'k':
				if (optarg) {	//
					authKey = optarg;
				}
				break;
			case 't':
					test = true;
					break;

			case 'v':
				logLevel--;
				if (optarg) {	//
					logLevel -= strlen(optarg);
				}
			break;
			case 'V':
				std::cout << "SiteProxy streamer: "<< VERSION << std::endl;
				std::cout << "built " << __DATE__ << " " __TIME__ << std::endl;
				exit(0);
			break;
		}
	}
	std::cout << "SiteProxy streamer: "<< VERSION << " built " << __DATE__ << " " __TIME__ << std::endl;


	rtc::LogMessage::LogToDebug((rtc::LoggingSeverity)logLevel);
	rtc::LogMessage::LogTimestamps();
	rtc::LogMessage::LogThreads();
	std::cout << "Logger level:" <<  rtc::LogMessage::GetLogToDebug() << std::endl;

	rtc::Thread* thread = rtc::Thread::Current();
	rtc::InitializeSSL();


	// webrtc server
	PeerConnectionManager webRtcServer(stunurl, turnurl, urlList, audioLayer);
	if (!webRtcServer.InitializePeerConnection())
	{
		std::cout << "Cannot Initialize WebRTC server" << std::endl;
	}
	else
	{
		// http server
		std::vector<std::string> options;
		options.push_back("listening_ports");
		options.push_back(httpAddress);

		options.push_back("access_control_allow_origin");
		options.push_back("*");


		if (test)
		{
			options.push_back("document_root");
			options.push_back("./html");
			webRtcServer.addStream("Test", "rtsp://video:only@bhlowe.com/cam/realmonitor?channel=1&subtype=1");
			std::cout << "Starting in test mode.. adding test stream and using .html";

		}

		try {
			std::cout << "HTTP Listen at " << httpAddress << std::endl;
			HttpServerRequestHandler httpServer(&webRtcServer, options, authKey);

			// start STUN server if needed
			std::unique_ptr<cricket::StunServer> stunserver;
			if (localstunurl != NULL)
			{
				rtc::SocketAddress server_addr;
				server_addr.FromString(localstunurl);
				rtc::AsyncUDPSocket* server_socket = rtc::AsyncUDPSocket::Create(thread->socketserver(), server_addr);
				if (server_socket)
				{
					stunserver.reset(new cricket::StunServer(server_socket));
					std::cout << "STUN Listening at " << server_addr.ToString() << std::endl;
				}
			}

			// mainloop
			thread->Run();

		} catch (const CivetException & ex) {
			std::cout << "Cannot Initialize start server exception:" << ex.what() << std::endl;
		}
	}

	rtc::CleanupSSL();
	return 0;
}
