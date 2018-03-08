/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** PeerConnectionManager.h
**
** -------------------------------------------------------------------------*/

#ifndef PEERCONNECTIONMANAGER_H_
#define PEERCONNECTIONMANAGER_H_

#define BHL 1

#include <string>

#include "api/peerconnectioninterface.h"
#include "api/test/fakeconstraints.h"

#include "modules/audio_device/include/audio_device.h"

#include "rtc_base/logging.h"
#include "rtc_base/json.h"


class PeerConnectionManager {

	// BHL
	class RTSPStream : public rtc::RefCountInterface
	{

		public:
			RTSPStream(std::string id) : id(id)
			{
			}

			const std::string id;
			rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track;
			rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track;
			rtc::scoped_refptr<webrtc::MediaStreamInterface> stream;
			std::unique_ptr<cricket::VideoCapturer> video_capturer;
			class RTSPVideoCapturer * rtspvideocapturer;
			int timeout;
			std::string error;
			std::string url;
			std::string transport;
			void setError(const std::string& e)
			{
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << " RTSPStream error :" << e;
				error = e;
			}

		protected:
		// the destructor may be called from any thread.
		~RTSPStream() {
			RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << " ~RTSPStream:" << id;
		}
	};



	class VideoSink : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
		public:
			VideoSink(webrtc::VideoTrackInterface* track): m_track(track) {
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << " track:" << m_track->id();
				m_track->AddOrUpdateSink(this, rtc::VideoSinkWants());
			}
			virtual ~VideoSink() {
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << " track:" << m_track->id();
				m_track->RemoveSink(this);
			}

			// VideoSinkInterface implementation
			virtual void OnFrame(const webrtc::VideoFrame& video_frame) {
				rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(video_frame.video_frame_buffer()->ToI420());
				RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << " frame:" << buffer->width() << "x" << buffer->height();
			}

		protected:
			rtc::scoped_refptr<webrtc::VideoTrackInterface> m_track;
	};

	class SetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
		public:
			static SetSessionDescriptionObserver* Create(webrtc::PeerConnectionInterface* pc)
			{
				return  new rtc::RefCountedObject<SetSessionDescriptionObserver>(pc);
			}
			virtual void OnSuccess()
			{
				std::string sdp;
				if (m_pc->local_description())
				{
					m_pc->local_description()->ToString(&sdp);
					RTC_LOG(INFO) << __PRETTY_FUNCTION__ << " Local SDP:" << sdp;
				}
				if (m_pc->remote_description())
				{
					m_pc->remote_description()->ToString(&sdp);
					RTC_LOG(INFO) << __PRETTY_FUNCTION__ << " Remote SDP:" << sdp;
				}
			}
			virtual void OnFailure(const std::string& error)
			{
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << " " << error;
			}
		protected:
			SetSessionDescriptionObserver(webrtc::PeerConnectionInterface* pc) : m_pc(pc) {};

		private:
			webrtc::PeerConnectionInterface* m_pc;
	};

	class CreateSessionDescriptionObserver : public webrtc::CreateSessionDescriptionObserver {
		public:
			static CreateSessionDescriptionObserver* Create(webrtc::PeerConnectionInterface* pc)
			{
				return  new rtc::RefCountedObject<CreateSessionDescriptionObserver>(pc);
			}
			virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc)
			{
				std::string sdp;
				desc->ToString(&sdp);
				RTC_LOG(INFO) << __PRETTY_FUNCTION__ << " type:" << desc->type() << " sdp:" << sdp;
				m_pc->SetLocalDescription(SetSessionDescriptionObserver::Create(m_pc), desc);
			}
			virtual void OnFailure(const std::string& error) {
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << " " << error;
			}
		protected:
			CreateSessionDescriptionObserver(webrtc::PeerConnectionInterface* pc) : m_pc(pc) {};

		private:
			webrtc::PeerConnectionInterface* m_pc;
	};

	class PeerConnectionStatsCollectorCallback : public webrtc::RTCStatsCollectorCallback {
		public:
			PeerConnectionStatsCollectorCallback() {}
			void clearReport() { m_report.clear(); }
			Json::Value getReport() { return m_report; }

		protected:
			virtual void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
				for (const webrtc::RTCStats& stats : *report) {
					Json::Value statsMembers;
					for (const webrtc::RTCStatsMemberInterface* member : stats.Members()) {
						statsMembers[member->name()] = member->ValueToString();
					}
					m_report[stats.id()] = statsMembers;
				}
				// bhlowe
				RTC_LOG(INFO) << __PRETTY_FUNCTION__ << " BHL: report:" << m_report;

			}

			Json::Value m_report;


	};

	class DataChannelObserver : public webrtc::DataChannelObserver  {
		public:
			DataChannelObserver(rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel): m_dataChannel(dataChannel) {
				m_dataChannel->RegisterObserver(this);

			}
			virtual ~DataChannelObserver() {
				m_dataChannel->UnregisterObserver();
			}

			// DataChannelObserver interface
			virtual void OnStateChange() {
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << " channel:" << m_dataChannel->label() << " state:"<< webrtc::DataChannelInterface::DataStateString(m_dataChannel->state());
				std::string msg(m_dataChannel->label() + " " + webrtc::DataChannelInterface::DataStateString(m_dataChannel->state()));
				webrtc::DataBuffer buffer(msg);
				m_dataChannel->Send(buffer);
			}
			virtual void OnMessage(const webrtc::DataBuffer& buffer) {
				std::string msg((const char*)buffer.data.data(),buffer.data.size());
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << " channel:" << m_dataChannel->label() << " msg:" << msg;
			}

		protected:
			rtc::scoped_refptr<webrtc::DataChannelInterface>    m_dataChannel;
	};




	class PeerConnectionObserver : public webrtc::PeerConnectionObserver {
		public:
			PeerConnectionObserver(PeerConnectionManager* peerConnectionManager, const std::string& peerid, const webrtc::PeerConnectionInterface::RTCConfiguration & config, const webrtc::FakeConstraints & constraints)
			: m_peerConnectionManager(peerConnectionManager)
			, m_peerid(peerid)
			, m_localChannel(NULL)
			, m_remoteChannel(NULL)
			, iceCandidateList_(Json::arrayValue) {
				m_pc = m_peerConnectionManager->peer_connection_factory_->CreatePeerConnection(config,
							    &constraints,
							    NULL,
							    NULL,
							    this);

				m_statsCallback = new rtc::RefCountedObject<PeerConnectionStatsCollectorCallback>();

				rtc::scoped_refptr<webrtc::DataChannelInterface>   channel = m_pc->CreateDataChannel("ServerDataChannel", NULL);
				m_localChannel = new DataChannelObserver(channel);
			};

			virtual ~PeerConnectionObserver() {
				RTC_LOG(INFO) << __PRETTY_FUNCTION__;
				delete m_localChannel;
				delete m_remoteChannel;
				m_pc->Close();
			}

			Json::Value getIceCandidateList() { return iceCandidateList_; 	}

			Json::Value getStats() {
				m_statsCallback->clearReport();
				m_pc->GetStats(m_statsCallback);
				int count=10;
				while ( (m_statsCallback->getReport().empty()) && (--count > 0) )
				{
					usleep(1000);
				}
				return Json::Value(m_statsCallback->getReport());
			};

			rtc::scoped_refptr<webrtc::PeerConnectionInterface> getPeerConnection() { return m_pc; };

			// PeerConnectionObserver interface
			virtual void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)    {
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << " nb video tracks:" << stream->GetVideoTracks().size();
				webrtc::VideoTrackVector videoTracks = stream->GetVideoTracks();
				if (videoTracks.size()>0) {
					m_videosink.reset(new VideoSink(videoTracks.at(0)));
				}
			}
			virtual void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__;
			}
			virtual void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__;
				m_remoteChannel = new DataChannelObserver(channel);
			}
			virtual void OnRenegotiationNeeded()                              {
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << " peerid:" << m_peerid;;
			}

			virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);

			virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState state) {
				RTC_LOG(LERROR) << __PRETTY_FUNCTION__ << " state:" << state << " peerid:" << m_peerid;
			}
			virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState state) {
				RTC_LOG(INFO) << __PRETTY_FUNCTION__ << " state:" << state  << " peerid:" << m_peerid;
				if ( (state == webrtc::PeerConnectionInterface::kIceConnectionFailed)
				   ||(state == webrtc::PeerConnectionInterface::kIceConnectionClosed) )
				{
					iceCandidateList_.clear();
					m_peerConnectionManager->hangUp(m_peerid);
				}
			}

			virtual void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState) {
			}


		private:
			PeerConnectionManager* m_peerConnectionManager;
			const std::string m_peerid;
			rtc::scoped_refptr<webrtc::PeerConnectionInterface> m_pc;
			DataChannelObserver*    m_localChannel;
			DataChannelObserver*    m_remoteChannel;
			Json::Value iceCandidateList_;
			rtc::scoped_refptr<PeerConnectionStatsCollectorCallback> m_statsCallback;
			std::unique_ptr<VideoSink>                               m_videosink;
	};

	public:
		PeerConnectionManager(const std::string & stunurl, const std::string & turnurl, const std::map<std::string,std::string> & urlList, const webrtc::AudioDeviceModule::AudioLayer audioLayer);
		virtual ~PeerConnectionManager();

		bool InitializePeerConnection();

		virtual const Json::Value getIceCandidateList(const std::string &peerid);
		virtual const Json::Value addIceCandidate(const std::string &peerid, const Json::Value& jmessage);
		// virtual const Json::Value getVideoDeviceList();

		virtual const Json::Value hangUp(const std::string &peerid);
		virtual const Json::Value call(const std::string &peerid, const std::string & streamID, const std::string & options, const Json::Value& jmessage);
		virtual const Json::Value getIceServers(const std::string& clientIp);
		virtual const Json::Value getPeerConnectionList();
		virtual const Json::Value getStreamList();


		const Json::Value createOffer(const std::string &peerid, const std::string & videourl, const std::string & audiourl, const std::string & options);
		void              setAnswer(const std::string &peerid, const Json::Value& jmessage);

	protected:
		PeerConnectionObserver*                 CreatePeerConnection(const std::string& peerid);
		// bool                                    AddStream(webrtc::PeerConnectionInterface* peer_connection, const std::string & streamID, const std::string & options);


		bool attachStream(webrtc::PeerConnectionInterface* peer_connection, const std::string & streamID);


		// bhl
		rtc::scoped_refptr<PeerConnectionManager::RTSPStream> getRTSPStream(const std::string & streamLabel, const std::string & options);

		bool      AddRTSPStream(webrtc::PeerConnectionInterface* peer_connection, const std::string & streamID, const std::string & options);
		// rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateRTSPStream(const std::string & videourl, const std::string & options);



		rtc::scoped_refptr<RTSPStream> CreateRTSPStream(const std::string & id, const std::string & rtspURL, const std::string & options);
		void closeStream(rtc::scoped_refptr<PeerConnectionManager::RTSPStream> stream);

		rtc::scoped_refptr<PeerConnectionManager::RTSPStream> getStream(const std::string & streamLabel);


		bool  				streamStillUsed(const std::string & streamLabel);

	protected:
		rtc::scoped_refptr<webrtc::AudioDeviceModule>                             audioDeviceModule_;
		rtc::scoped_refptr<webrtc::AudioDecoderFactory>                           audioDecoderfactory_;
		rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>                peer_connection_factory_;
		std::map<std::string, PeerConnectionObserver* >                           peer_connectionobs_map_;



		std::map<std::string, rtc::scoped_refptr<RTSPStream> >                   rtsp_map_;		// bhl123
		// std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >  stream_map_;



		std::string                                                               stunurl_;
		std::string                                                               turnurl_;
		std::string                                                               turnuser_;
		std::string                                                               turnpass_;
		std::map<std::string,std::string>                                   			urlList_;

	public:

		std::map<std::string, std::string> tokenMap;		// token, stream_name		token is unique random string. stream_name is valid stream name/id


		virtual bool hasToken(const std::string &token, const std::string &stream_name);


		virtual bool hasStream(const std::string &stream_name);
		virtual const Json::Value listStreams();
		virtual const Json::Value addStream(const std::string &stream_name, const std::string &url, const std::string &transport);
		virtual const Json::Value removeStream(const std::string &stream_name);

		virtual const Json::Value addToken(const std::string &token, const std::string &stream_name);
		virtual const Json::Value removeToken(const std::string &token);
		virtual const Json::Value listTokens();
		static  const Json::Value error(const std::string &err);
		virtual const Json::Value test();
		static const Json::Value success();

};

#endif
