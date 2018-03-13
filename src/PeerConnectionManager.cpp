/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** PeerConnectionManager.cpp
**
** -------------------------------------------------------------------------*/

#include <iostream>
#include <fstream>
#include <utility>

#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "modules/video_capture/video_capture_factory.h"
#include "media/engine/webrtcvideocapturerfactory.h"

#include "PeerConnectionManager.h"
#include "CivetServer.h"
#include "rtspvideocapturer.h"
#include <net/if.h>
#include <ifaddrs.h>


const char kAudioLabel[] = "audio_label";
const char kVideoLabel[] = "video_label";

// Names used for a IceCandidate JSON object.
const char kCandidateSdpMidName[] = "sdpMid";
const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
const char kCandidateSdpName[] = "candidate";

// Names used for a SessionDescription JSON object.
const char kSessionDescriptionTypeName[] = "type";
const char kSessionDescriptionSdpName[] = "sdp";

#undef RTC_LOG
#define RTC_LOG(x) std::cout

/* ---------------------------------------------------------------------------
**  Constructor
** -------------------------------------------------------------------------*/
PeerConnectionManager::PeerConnectionManager(const std::string & stunurl, const std::string & turnurl, const std::map<std::string,std::string> & urlList, const webrtc::AudioDeviceModule::AudioLayer audioLayer)
	: audioDeviceModule_(webrtc::AudioDeviceModule::Create(0, audioLayer))
	, audioDecoderfactory_(webrtc::CreateBuiltinAudioDecoderFactory())
	, peer_connection_factory_(webrtc::CreatePeerConnectionFactory(NULL,
                                                                    rtc::Thread::Current(),
                                                                    NULL,
                                                                    audioDeviceModule_,	//
                                                                    webrtc::CreateBuiltinAudioEncoderFactory(),
                                                                    audioDecoderfactory_,
                                                                    NULL,
                                                                    NULL))
	, stunurl_(stunurl)
	, turnurl_(turnurl)
	, urlList_(urlList)
{
	if (turnurl_.length() > 0)
	{
		std::size_t pos = turnurl_.find('@');
		if (pos != std::string::npos)
		{
			std::string credentials = turnurl_.substr(0, pos);
			turnurl_ = turnurl_.substr(pos + 1);
			pos = credentials.find(':');
			if (pos == std::string::npos)
			{
				turnuser_ = credentials;
			}
			else
			{
				turnuser_ = credentials.substr(0, pos);
				turnpass_ = credentials.substr(pos + 1);
			}
		}
	}

}

/* ---------------------------------------------------------------------------
**  Destructor
** -------------------------------------------------------------------------*/
PeerConnectionManager::~PeerConnectionManager()
{
}


std::string getServerIpFromClientIp(int clientip)
{
	std::string serverAddress;
	char host[NI_MAXHOST];
	struct ifaddrs *ifaddr = NULL;
	if (getifaddrs(&ifaddr) == 0)
	{
		for (struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
		{
			if ( (ifa->ifa_netmask != NULL) && (ifa->ifa_netmask->sa_family == AF_INET) && (ifa->ifa_addr != NULL) && (ifa->ifa_addr->sa_family == AF_INET) )
			{
				struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
				struct sockaddr_in* mask = (struct sockaddr_in*)ifa->ifa_netmask;
				if ( (addr->sin_addr.s_addr & mask->sin_addr.s_addr) == (clientip & mask->sin_addr.s_addr) )
				{
					if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, sizeof(host), NULL, 0, NI_NUMERICHOST) == 0)
					{
						serverAddress = host;
						break;
					}
				}
			}
		}
	}
	freeifaddrs(ifaddr);
	return serverAddress;
}

/* ---------------------------------------------------------------------------
**  return iceServers as JSON vector
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getIceServers(const std::string& clientIp)
{
	Json::Value url;
	std::string stunurl("stun:");
	if (stunurl_.find("0.0.0.0:") == 0) {
		// answer with ip that is on same network as client
		stunurl += getServerIpFromClientIp(inet_addr(clientIp.c_str()));
		stunurl += stunurl_.substr(stunurl_.find_first_of(':'));
	} else {
		stunurl += stunurl_;
	}
	url["url"] = stunurl;

	Json::Value urls;
	urls.append(url);

	if (turnurl_.length() > 0)
	{
		Json::Value turn;
		turn["url"] = "turn:" + turnurl_;
		if (turnuser_.length() > 0) turn["username"] = turnuser_;
		if (turnpass_.length() > 0) turn["credential"] = turnpass_;
		urls.append(turn);
	}

	Json::Value iceServers;
	iceServers["iceServers"] = urls;

	return iceServers;
}

/* ---------------------------------------------------------------------------
**  add ICE candidate to a PeerConnection
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::addIceCandidate(const std::string& peerid, const Json::Value& jmessage)
{
	bool result = false;
	std::string sdp_mid;
	int sdp_mlineindex = 0;
	std::string sdp;
	if (  !rtc::GetStringFromJsonObject(jmessage, kCandidateSdpMidName, &sdp_mid)
	   || !rtc::GetIntFromJsonObject(jmessage, kCandidateSdpMlineIndexName, &sdp_mlineindex)
	   || !rtc::GetStringFromJsonObject(jmessage, kCandidateSdpName, &sdp))
	{
		RTC_LOG(WARNING) << "Can't parse received message:" << jmessage;
	}
	else
	{
		std::unique_ptr<webrtc::IceCandidateInterface> candidate(webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp, NULL));
		if (!candidate.get())
		{
			RTC_LOG(WARNING) << "Can't parse received candidate message.";
		}
		else
		{
			std::map<std::string, PeerConnectionObserver* >::iterator  it = peer_connectionobs_map_.find(peerid);
			if (it != peer_connectionobs_map_.end())
			{
				rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = it->second->getPeerConnection();
				if (!peerConnection->AddIceCandidate(candidate.get()))
				{
					RTC_LOG(WARNING) << "Failed to apply the received candidate";
				}
				else
				{
					result = true;
				}
			}
		}
	}
	Json::Value answer;
	if (result) {
		answer = result;
	}
	return answer;
}




/* ---------------------------------------------------------------------------
** set answer to a call initiated by createOffer
** -------------------------------------------------------------------------*/
void PeerConnectionManager::setAnswer(const std::string &peerid, const Json::Value& jmessage)
{
	RTC_LOG(INFO) << "PCM::setAnswer "<< jmessage;

	std::string type;
	std::string sdp;
	if (  !rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type)
	   || !rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp))
	{
		RTC_LOG(WARNING) << "Can't parse received message.";
	}
	else
	{
		webrtc::SessionDescriptionInterface* session_description(webrtc::CreateSessionDescription(type, sdp, NULL));
		if (!session_description)
		{
			RTC_LOG(WARNING) << "Can't parse received session description message.";
		}
		else
		{
			RTC_LOG(LERROR) << "From peerid:" << peerid << " received session description :" << session_description->type();

			std::map<std::string, PeerConnectionObserver* >::iterator  it = peer_connectionobs_map_.find(peerid);
			if (it != peer_connectionobs_map_.end())
			{
				rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = it->second->getPeerConnection();
				peerConnection->SetRemoteDescription(SetSessionDescriptionObserver::Create(peerConnection), session_description);
			}
		}
	}
}

/* ---------------------------------------------------------------------------
**  auto-answer to a call
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::call(const std::string & peerid, const std::string & streamID, const std::string & options, const Json::Value& jmessage)
{
	RTC_LOG(INFO) << __FUNCTION__ <<" bhl PeerConnectionManager::call";
	Json::Value answer;

	std::string type;
	std::string sdp;

	if (  !rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type)
	   || !rtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp))
	{
		RTC_LOG(WARNING) << "Can't parse received message.";
	}
	else
	{
		PeerConnectionObserver* peerConnectionObserver = this->CreatePeerConnection(peerid);
		if (!peerConnectionObserver)
		{
			RTC_LOG(LERROR) << "Failed to initialize PeerConnection";
		}
		else
		{
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = peerConnectionObserver->getPeerConnection();

			// set bandwidth
			std::string tmp;
			if (CivetServer::getParam(options, "bitrate", tmp)) {
				int bitrate = std::stoi(tmp);

				webrtc::PeerConnectionInterface::BitrateParameters bitrateParam;
				bitrateParam.min_bitrate_bps = rtc::Optional<int>(bitrate/2);
				bitrateParam.current_bitrate_bps = rtc::Optional<int>(bitrate);
				bitrateParam.max_bitrate_bps = rtc::Optional<int>(bitrate*2);
				peerConnection->SetBitrate(bitrateParam);

				RTC_LOG(WARNING) << "set bitrate:" << bitrate;
			}


			RTC_LOG(INFO) << "nbStreams local:" << peerConnection->local_streams()->count() << " remote:" << peerConnection->remote_streams()->count() << " localDescription:" << peerConnection->local_description();

			// register peerid
			peer_connectionobs_map_.insert(std::pair<std::string, PeerConnectionObserver* >(peerid, peerConnectionObserver));

			// set remote offer
			webrtc::SessionDescriptionInterface* session_description(webrtc::CreateSessionDescription(type, sdp, NULL));
			if (!session_description)
			{
				RTC_LOG(WARNING) << "Can't parse received session description message.";
			}
			else
			{
				peerConnection->SetRemoteDescription(SetSessionDescriptionObserver::Create(peerConnection), session_description);
			}




			rtc::scoped_refptr<PeerConnectionManager::RTSPStream> rtsp_stream = getRTSPStream(streamID, options);

			if (!rtsp_stream.get())
			{
				return this->error("Can't add rtsp_stream ");
			}

			if (!peerConnection->AddStream(rtsp_stream->stream))
			{
				RTC_LOG(LS_ERROR) << "Adding stream to PeerConnection failed";
				return this->error("peerConnection->AddStream failed ");
			}

			// attachStream(peerConnection, rtsp_stream);


			// add local stream

			// create answer
			webrtc::FakeConstraints constraints;
			constraints.AddMandatory(webrtc::MediaConstraintsInterface::kOfferToReceiveVideo, "false");
			constraints.AddMandatory(webrtc::MediaConstraintsInterface::kOfferToReceiveAudio, "false");
			peerConnection->CreateAnswer(CreateSessionDescriptionObserver::Create(peerConnection), &constraints);

			RTC_LOG(INFO) << "nbStreams local:" << peerConnection->local_streams()->count() << " remote:" << peerConnection->remote_streams()->count()
					<< " localDescription:" << peerConnection->local_description()
					<< " remoteDescription:" << peerConnection->remote_description();

			// waiting for answer
			int count=10;
			while ( (peerConnection->local_description() == NULL) && (--count > 0) )
			{
				usleep(1000);
			}

			RTC_LOG(INFO) << "nbStreams local:" << peerConnection->local_streams()->count() << " remote:" << peerConnection->remote_streams()->count()
					<< " localDescription:" << peerConnection->local_description()
					<< " remoteDescription:" << peerConnection->remote_description();

			// return the answer
			const webrtc::SessionDescriptionInterface* desc = peerConnection->local_description();
			if (desc)
			{
				std::string sdp;
				desc->ToString(&sdp);

				answer[kSessionDescriptionTypeName] = desc->type();
				answer[kSessionDescriptionSdpName] = sdp;
			}
			else
			{
				RTC_LOG(LERROR) << "Failed to create answer";
			}
		}
	}
	return answer;
}

bool PeerConnectionManager::streamStillUsed(const std::string & streamLabel)
{
	bool stillUsed = false;
	for (auto it: peer_connectionobs_map_)
	{
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = it.second->getPeerConnection();
		rtc::scoped_refptr<webrtc::StreamCollectionInterface> localstreams (peerConnection->local_streams());
		for (unsigned int i = 0; i<localstreams->count(); i++)
		{
			if (localstreams->at(i)->label() == streamLabel)
			{
				stillUsed = true;
				break;
			}
		}
	}
	return stillUsed;
}


void PeerConnectionManager::closeStream(rtc::scoped_refptr<PeerConnectionManager::RTSPStream> stream)
{
	rtc::scoped_refptr<webrtc::MediaStreamInterface> media = stream->stream;
	// remove video tracks
	while (media->GetVideoTracks().size() > 0)
	{
		media->RemoveTrack(media->GetVideoTracks().at(0));
	}
	// remove audio tracks
	while (media->GetAudioTracks().size() > 0)
	{
		media->RemoveTrack(media->GetAudioTracks().at(0));
	}

	media.release();

	auto it = rtsp_map_.find(stream->id);
	rtsp_map_.erase(it);
}

/* ---------------------------------------------------------------------------
**  hangup a call
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::hangUp(const std::string &peerid)
{
	bool result = false;
	RTC_LOG(INFO) << __FUNCTION__ << " " << peerid;

	std::map<std::string, PeerConnectionObserver* >::iterator  it = peer_connectionobs_map_.find(peerid);
	if (it != peer_connectionobs_map_.end())
	{
		RTC_LOG(LS_ERROR) << "Close PeerConnection";
		PeerConnectionObserver* pcObserver = it->second;
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = pcObserver->getPeerConnection();
		peer_connectionobs_map_.erase(it);

		rtc::scoped_refptr<webrtc::StreamCollectionInterface> localstreams (peerConnection->local_streams());
		Json::Value streams;
		for (unsigned int i = 0; i<localstreams->count(); i++)
		{
			std::string streamLabel = localstreams->at(i)->label();

			bool stillUsed = this->streamStillUsed(streamLabel);
			if (!stillUsed)
			{
				RTC_LOG(LS_ERROR) << "Close PeerConnection no more used " << streamLabel;
				#if 0
				std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >::iterator it = stream_map_.find(streamLabel);
				if (it != stream_map_.end())
				{
					rtc::scoped_refptr<webrtc::MediaStreamInterface> media = it->second;
					// remove video tracks
					while (media->GetVideoTracks().size() > 0)
					{
						media->RemoveTrack(media->GetVideoTracks().at(0));
					}
					// remove audio tracks
					while (media->GetAudioTracks().size() > 0)
					{
						media->RemoveTrack(media->GetAudioTracks().at(0));
					}

					media.release();
					stream_map_.erase(it);
				}
				#else

					this->closeStream(getStream(streamLabel));

				#endif

			}
		}

		delete pcObserver;
		result = true;
	}
	Json::Value answer;
	if (result) {
		answer = result;
	}
	return answer;
}


/* ---------------------------------------------------------------------------
**  get list ICE candidate associayed with a PeerConnection
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getIceCandidateList(const std::string &peerid)
{
	RTC_LOG(INFO) << __FUNCTION__;

	Json::Value value;
	std::map<std::string, PeerConnectionObserver* >::iterator  it = peer_connectionobs_map_.find(peerid);
	if (it != peer_connectionobs_map_.end())
	{
		PeerConnectionObserver* obs = it->second;
		if (obs)
		{
			value = obs->getIceCandidateList();
		}
		else
		{
			RTC_LOG(LS_ERROR) << "No observer for peer:" << peerid;
		}
	}
	return value;
}

/* ---------------------------------------------------------------------------
**  get PeerConnection list
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getPeerConnectionList()
{
	Json::Value value(Json::arrayValue);
	for (auto it : peer_connectionobs_map_)
	{
		Json::Value content;

		// get local SDP
		rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = it.second->getPeerConnection();
		if ( (peerConnection) && (peerConnection->local_description()) ) {
			std::string sdp;
			peerConnection->local_description()->ToString(&sdp);
			content["sdp"] = sdp;

			Json::Value streams;
			rtc::scoped_refptr<webrtc::StreamCollectionInterface> localstreams (peerConnection->local_streams());
			if (localstreams) {
				for (unsigned int i = 0; i<localstreams->count(); i++) {
					if (localstreams->at(i)) {
						Json::Value tracks;

						const webrtc::VideoTrackVector& videoTracks = localstreams->at(i)->GetVideoTracks();
						for (unsigned int j=0; j<videoTracks.size() ; j++)
						{
							Json::Value track;
							tracks[videoTracks.at(j)->kind()].append(videoTracks.at(j)->id());
						}
						const webrtc::AudioTrackVector& audioTracks = localstreams->at(i)->GetAudioTracks();
						for (unsigned int j=0; j<audioTracks.size() ; j++)
						{
							Json::Value track;
							tracks[audioTracks.at(j)->kind()].append(audioTracks.at(j)->id());
						}

						Json::Value stream;
						stream[localstreams->at(i)->label()] = tracks;

						streams.append(stream);
					}
				}
			}
			content["streams"] = streams;
		}

		// get Stats
		content["stats"] = it.second->getStats();

		Json::Value pc;
		pc[it.first] = content;
		value.append(pc);
	}
	return value;
}

/* ---------------------------------------------------------------------------
**  get StreamList list
** -------------------------------------------------------------------------*/
const Json::Value PeerConnectionManager::getStreamList()
{
	Json::Value value(Json::arrayValue);
	for (auto it : rtsp_map_)
	{

		rtc::scoped_refptr<PeerConnectionManager::RTSPStream> stream  = it.second;
		Json::Value item;
		item["id"]=stream->id;
		item["error"]=stream->error;

		if (stream->rtspvideocapturer)
		{
			item["rtsp_stream"] = stream->rtspvideocapturer->getJSON();
		}
		/*
		item["xxx"]=xxx;
		rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track;
		rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track;
		rtc::scoped_refptr<webrtc::MediaStreamInterface> stream;
		std::unique_ptr<cricket::VideoCapturer> video_capturer;
		class RTSPVideoCapturer * rtspvideocapturer;
		int timeout;
		std::string error;
*/


			value.append(item);
	}
	return value;
}



/* ---------------------------------------------------------------------------
**  check if factory is initialized
** -------------------------------------------------------------------------*/
bool PeerConnectionManager::InitializePeerConnection()
{
	return (peer_connection_factory_.get() != NULL);
}

/* ---------------------------------------------------------------------------
**  create a new PeerConnection
** -------------------------------------------------------------------------*/
PeerConnectionManager::PeerConnectionObserver* PeerConnectionManager::CreatePeerConnection(const std::string& peerid)
{
	webrtc::PeerConnectionInterface::RTCConfiguration config;
	webrtc::PeerConnectionInterface::IceServer server;
	server.uri = "stun:" + stunurl_;
	server.username = "";
	server.password = "";
	config.servers.push_back(server);

	if (turnurl_.length() > 0)
	{
		webrtc::PeerConnectionInterface::IceServer turnserver;
		turnserver.uri = "turn:" + turnurl_;
		turnserver.username = turnuser_;
		turnserver.password = turnpass_;
		config.servers.push_back(turnserver);
	}

	webrtc::FakeConstraints constraints;
	constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, "true");

	PeerConnectionObserver* obs = new PeerConnectionObserver(this, peerid, config, constraints);
	if (!obs)
	{
		RTC_LOG(LERROR) << __FUNCTION__ << "CreatePeerConnection failed";
	}
	return obs;
}



rtc::scoped_refptr<PeerConnectionManager::RTSPStream> PeerConnectionManager::CreateRTSPStream(const std::string & id, const std::string & rtspURL, const std::string & options)
{

	rtc::scoped_refptr<PeerConnectionManager::RTSPStream> stream = new rtc::RefCountedObject<PeerConnectionManager::RTSPStream>(id);
	// PeerConnectionManager::RTSPStream * stream = new rtc::RefCountedObject<PeerConnectionStatsCollectorCallback>();


	if (!rtspURL.find("rtsp://") == 0) return stream;

	stream->timeout = 10;

	// Create Video Track
	RTC_LOG(INFO) << "PeerConnectionManager::CreateRTSPStream rtspURL:" << rtspURL << " options:" << options;
	rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track = NULL;



	std::string tmp;
	if (CivetServer::getParam(options, "timeout", tmp)) {
		stream->timeout = std::stoi(tmp);
	}
	// added hack to allow rtp transport by adding #tcp, #http, or #multicast to rtsp url.
	std::string::size_type ch = rtspURL.rfind('#');
	stream->transport="";
	stream->url = rtspURL;

	if (ch != std::string::npos)
	{
		stream->transport = rtspURL.substr(ch+1);
		stream->url = rtspURL.substr(0, ch);
	}

	RTC_LOG(INFO) << "CreateRTSPStream rtsp= "<< stream->url << " rtptransport=" << stream->transport << std::endl;

	stream->rtspvideocapturer = new RTSPVideoCapturer(stream->url, stream->timeout, stream->transport);

	int fps = 15;
	if (CivetServer::getParam(options, "fps", tmp)) {
		fps = std::stoi(tmp);
	}

	stream->rtspvideocapturer->fps = fps;

	// set capturer object.
	stream->video_capturer.reset(stream->rtspvideocapturer);




	if (!stream->video_capturer)
	{
		RTC_LOG(LS_ERROR) << " **** Cannot create capturer video:" << rtspURL;
		stream->setError("Cannot create capturer");
	}
	else
	{
		rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> videoSource = peer_connection_factory_->CreateVideoSource(std::move(stream->video_capturer), NULL);
		stream->video_track = peer_connection_factory_->CreateVideoTrack(kVideoLabel, videoSource);
	}

// create audio track

	RTC_LOG(INFO) << "CreateAudioTrack: " << " options:" << options;

	audioDeviceModule_->Terminate();
	rtc::scoped_refptr<RTSPAudioSource> audioSource = RTSPAudioSource::Create(audioDecoderfactory_, rtspURL);
	stream->audio_track = peer_connection_factory_->CreateAudioTrack(kAudioLabel, audioSource);



	stream->stream = peer_connection_factory_->CreateLocalMediaStream(id);
	if (!stream->stream.get())
	{
		RTC_LOG(LS_ERROR) << "Cannot create stream";
		stream->setError("stream->stream.get failed");
	}

	if (!stream->stream->AddTrack(stream->video_track) )
	{
		stream->setError("VideoTrack to MediaStream failed");
		RTC_LOG(LS_ERROR) << "Adding VideoTrack to MediaStream failed";
	}

	if ( (stream->audio_track) && (!stream->stream->AddTrack(stream->audio_track)) )
	{
		stream->setError("AudioTrack to MediaStream failed");
		RTC_LOG(LS_ERROR) << "Adding AudioTrack to MediaStream failed";
	}

	// stream_map_[id] = stream->stream;		// Eventually take out.
	return stream;
}


rtc::scoped_refptr<PeerConnectionManager::RTSPStream> PeerConnectionManager::getStream(const std::string & streamLabel)
{
	auto rit = rtsp_map_.find(streamLabel);
	if (rit != rtsp_map_.end())
		return  rit->second;
	return NULL;
}

// get existing, or create new RTSPStream
rtc::scoped_refptr<PeerConnectionManager::RTSPStream> PeerConnectionManager::getRTSPStream(const std::string & streamLabel, const std::string & options)
{
	rtc::scoped_refptr<PeerConnectionManager::RTSPStream> ret;

	auto rit = rtsp_map_.find(streamLabel);
	if (rit != rtsp_map_.end()) {
		ret = rit->second;
	} else
	{
		// need to create it.

		std::string rtspURL;
		auto videoit = urlList_.find(streamLabel);
		if (videoit != urlList_.end()) {
			rtspURL = videoit->second;
		}

		RTC_LOG(INFO) << "bhl AddStreams streamLabel="<< streamLabel<<" rtspURL=" << rtspURL << " options="<<options;

		if (rtspURL.empty())
		{
			RTC_LOG(LS_ERROR) << "bhl PeerConnectionManager::getRTSPStream failed for "<<streamLabel << " list="<< listStreams();
			return ret;
		}

		ret = this->CreateRTSPStream(streamLabel, rtspURL, options);
		rtsp_map_[streamLabel] = ret;
	}

	return ret;
}


/* ---------------------------------------------------------------------------
**  ICE callback
** -------------------------------------------------------------------------*/
void PeerConnectionManager::PeerConnectionObserver::OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
{
	RTC_LOG(INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index();

	std::string sdp;
	if (!candidate->ToString(&sdp))
	{
		RTC_LOG(LS_ERROR) << "Failed to serialize candidate";
	}
	else
	{
		RTC_LOG(INFO) << sdp;
		Json::Value jmessage;
		jmessage[kCandidateSdpMidName] = candidate->sdp_mid();
		jmessage[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();
		jmessage[kCandidateSdpName] = sdp;
		iceCandidateList_.append(jmessage);
	}
}




bool PeerConnectionManager::hasStream(const std::string &stream_name)
{
	int count = urlList_.count(stream_name);
	assert(count == 1 || count ==0);
	return count >0;
}

const Json::Value PeerConnectionManager::error(const std::string &err)
{
	Json::Value value;
	value["error"] = err;
	value["success"] = false;
	return value;
}

const Json::Value PeerConnectionManager::success()
{
	Json::Value value;
	value["success"] = true;
	return value;
}

const Json::Value PeerConnectionManager::listStreams()
{
	Json::Value value(Json::arrayValue);
	for (auto url : urlList_)
	{
		Json::Value entry;
		entry["name"] = url.first;
		entry["url"] = url.second;
		value.append(entry);
	}
	return value;
}


const Json::Value PeerConnectionManager::addStream(const std::string &stream_name, const std::string &url, const std::string &transport)
{
	if (hasStream("stream_name"))
			return error("stream already defined");

	if (!transport.empty())
	{
		std::string u = url;
		u.append("#").append(transport);
		RTC_LOG(INFO) << __FUNCTION__ << " bhl addStream with rtpTransport="<<u<< " transport="<<transport;
		urlList_[stream_name]=u;
	}
	else
	{
		urlList_[stream_name]=url;
	}

	return success();
}

const Json::Value PeerConnectionManager::removeStream(const std::string &stream_name)
{
	// TODO: Stop and remove the stream if it is "streaming".
	std::map<std::string, std::string >::iterator  it = urlList_.find(stream_name);
	if (it != urlList_.end())
	{
		urlList_.erase(it);
		return success();
  }
	return error("removeStream: stream_name not found");
}

// BHL
bool PeerConnectionManager::hasToken(const std::string &token, const std::string &stream_name)
{
 bool has = hasStream(stream_name);
 assert(has);

 std::map<std::string,std::string>::iterator it = tokenMap.find(token);
 if (it != tokenMap.end())
 {
		if (has && stream_name.compare(it->second)==0)
			return true;
 }
 return false;
}

const Json::Value PeerConnectionManager::addToken(const std::string &token, const std::string &stream_name)
{
	RTC_LOG(LS_ERROR) << "addToken token:"<<token<< " stream:" << stream_name;
	tokenMap.insert(std::pair<std::string, std::string >(token, stream_name));

	assert(hasToken(token, stream_name));

	return success();
}


const Json::Value PeerConnectionManager::removeToken(const std::string &token)
{
	std::map<std::string, std::string >::iterator  it = tokenMap.find(token);
	if (it != tokenMap.end())
	{
		RTC_LOG(LS_ERROR) << "removeToken "<<token;
		tokenMap.erase(it);
		// disconnect the user.
		hangUp(token);

		return success();
 	 }

	return error("token not found");
}

const Json::Value PeerConnectionManager::listTokens()
{
	Json::Value value(Json::arrayValue);
	for (auto token : tokenMap)
	{
		Json::Value e;
		e["token"] = token.first;
		e["stream_name"] = token.second;
		value.append(e);
	}
	return value;
}

const Json::Value PeerConnectionManager::test()
{
	Json::Value value(Json::arrayValue);



		for (auto it: peer_connectionobs_map_)
		{
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection = it.second->getPeerConnection();
			rtc::scoped_refptr<webrtc::StreamCollectionInterface> localstreams (peerConnection->local_streams());
			Json::Value e;
			// e["signaling_state"] = peerConnection.signaling_state();
			e["count"] = (int) localstreams->count();
			value.append(e);


			for (unsigned int i = 0; i<localstreams->count(); i++)
			{
				localstreams->at(i)->label();

			}
		}


	return value;
}
