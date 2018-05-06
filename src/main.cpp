/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** main.cpp
**
** -------------------------------------------------------------------------*/

#include <iostream>
#include <fstream>
#include "rtc_base/ssladapter.h"
#include "rtc_base/thread.h"
#include "p2p/base/stunserver.h"

#include "PeerConnectionManager.h"
#include "HttpServerRequestHandler.h"


/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[])
{
	const char* turnurl       = "";	//
	const char* localstunurl  = NULL;
	const char* stunurl       = "stun.l.google.com:19302";
	int logLevel              = rtc::LS_ERROR;	// rtc::LS_INFO (2), rtc::LS_ERROR (4), LS_VERBOSE;
  // LS_ERROR, LS_NONE, INFO = LS_INFO, WARNING = LS_WARNING,
	// LS_SENSITIVE, LS_VERBOSE, LS_INFO, LS_WARNING,

	webrtc::AudioDeviceModule::AudioLayer audioLayer = webrtc::AudioDeviceModule::kDummyAudio;
	std::cout << "info:"<<rtc::LS_INFO<< " err:"<< rtc::LS_ERROR<<" verbose:"<<  rtc::LS_VERBOSE;

	std::string configFile = "config.json";

	int defaultPort = 8000;

	std::string authKey = "";

	// const char * port = getenv("PORT");
	// if (port)
	// {
	// 	httpPort = port;
	// }
	// httpAddress.append(httpPort);

	int c = 0;
	while ((c = getopt (argc, argv, "kfVv:k:" "c:H:w:" "t:S::s::" "a::n:u:")) != -1)
	{
		switch (c)
		{
			case 'k':
				if (optarg) {	//
					authKey = optarg;
				}
				break;
			case 'f':
					if (optarg) {	//
						configFile = optarg;
					}
					break;
			case 'v':
				logLevel--;
				if (optarg) {	//
					logLevel = atoi(optarg);
					// logLevel -= strlen(optarg);
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

	std::cout << "loading config "<< std::endl;
	std::ifstream file(configFile);
	Json::Value config;

	if (file.good())
	{
		file >> config;
		// std::cout << "config="<<config.toStyledString()<< std::endl;

	} else {
		std::cout << "No config... using defaults"<< std::endl;

		config["turnurl"] = turnurl;
		config["stunurl"] = stunurl;
		Json::Value httpOptions;
		httpOptions["listening_ports"] = defaultPort;

		config["httpOptions"] = httpOptions;
	}

	// allow override of authKey from cmd line
	if (!authKey.empty())
		config["authKey"] = authKey;

	// webrtc server
	PeerConnectionManager webRtcServer(config, audioLayer);
	if (!webRtcServer.InitializePeerConnection())
	{
		std::cout << "Cannot Initialize WebRTC server" << std::endl;
	}
	else
	{

		if (!config["fps"].empty())
		{
			int fps = config["fps"].asInt();
			webRtcServer.setFPS(fps);

		}
		std::cout << "fps="<< webRtcServer.getFPS() << std::endl;



		// http server

		std::vector<std::string> options;
		Json::Value httpOptions = config["httpOptions"];
		std::cout << "httpOptions="<<httpOptions.toStyledString()<< std::endl;

		// Json::ValueIterator
		for( Json::ValueIterator itr = httpOptions.begin() ; itr != httpOptions.end() ; itr++ )
		{
			Json::Value key = itr.key();
			Json::Value value = (*itr);
			// std::cout << "kv:" << key.asString() << "=" <<value.asString() <<std::endl;
			if (!key.asString().empty() && !value.asString().empty())
			{
				options.push_back(key.asString());
				options.push_back(value.asString());
			} else
			{
				std::cout << "warning: skip httpOption:" << key.asString() << "=" <<value.asString() <<std::endl;
			}
		}


		try {

			HttpServerRequestHandler httpServer(&webRtcServer, options, authKey);
			// start STUN server if needed

			// TODO Check stun/turn
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
