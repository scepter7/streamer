/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspvideocapturer.cpp
**
** -------------------------------------------------------------------------*/

#ifdef HAVE_LIVE555

#include "rtc_base/timeutils.h"
#include "rtc_base/logging.h"

#include "modules/video_coding/h264_sprop_parameter_sets.h"
#include "api/video/i420_buffer.h"

#include "libyuv/video_common.h"
#include "libyuv/convert.h"

#include "rtspvideocapturer.h"
#include <csignal> // bhl

uint8_t marker[] = { 0, 0, 0, 1};

// #undef LS_VERBOSE
// #define LS_VERBOSE INFO

int decodeRTPTransport(const std::string & rtpTransportString)
{
	int rtptransport = RTSPConnection::RTPUDPUNICAST;
	if (rtpTransportString == "tcp") {
		rtptransport = RTSPConnection::RTPOVERTCP;
	} else if (rtpTransportString == "http") {
		rtptransport = RTSPConnection::RTPOVERHTTP;
	} else if (rtpTransportString == "multicast") {
		rtptransport = RTSPConnection::RTPUDPMULTICAST;
	}

	// TESTING.. BHL
	// rtptransport = RTSPConnection::RTPOVERTCP;

	return rtptransport;
}

RTSPVideoCapturer::RTSPVideoCapturer(rtc::scoped_refptr<RTSPSource> source)
	: m_connection(m_env, this, source->getURL().c_str(), source->getTimeout(), decodeRTPTransport(source->getTransport()), 1)
{
	RTC_LOG(INFO) << "RTSPVideoCapturer" << source->toString();
	m_h264 = h264_new();

	// this->json["url"] = uri;
	// json["transport"] = rtptransport;
  decodedFrames=0;
	bytesReceived=0;
	goodPackets=0;
	badPackets=0;
	fps = source->getFPS();

}

RTSPVideoCapturer::~RTSPVideoCapturer()
{
	RTC_LOG(INFO) << "~RTSPVideoCapturer" << this << " m_h264 " << m_h264;

	h264_free(m_h264);
}


const Json::Value RTSPVideoCapturer::getJSON()
{
	Json::Value json;
	json["bytesReceived"] = (Json::Value::UInt64) bytesReceived;
	json["goodPackets"] = (Json::Value::UInt64) goodPackets;
	json["badPackets"] = (Json::Value::UInt64) badPackets;
	json["decodedFrames"] = (Json::Value::UInt64) decodedFrames;

	return json;
}



bool RTSPVideoCapturer::onNewSession(const char* id,const char* media, const char* codec, const char* sdp)
{
	bool success = false;
	if (strcmp(media, "video") == 0) {
		RTC_LOG(INFO) << "RTSPVideoCapturer::onNewSession " << media << "/" << codec << " " << sdp;

		if (strcmp(codec, "H264") == 0)
		{
			m_codec = codec;
			const char* pattern="sprop-parameter-sets=";
			const char* sprop=strstr(sdp, pattern);
			if (sprop)
			{
				std::string sdpstr(sprop+strlen(pattern));
				size_t pos = sdpstr.find_first_of(" ;\r\n");
				if (pos != std::string::npos)
				{
					sdpstr.erase(pos);
				}

				RTC_LOG(INFO) << "(BHL) RTSPVideoCapturer::onNewSession sdpstr " << sdpstr;

				webrtc::H264SpropParameterSets sprops;
				if (sprops.DecodeSprop(sdpstr))
				{
					struct timeval presentationTime;
					timerclear(&presentationTime);

					std::vector<uint8_t> sps;
					sps.insert(sps.end(), marker, marker+sizeof(marker));
					sps.insert(sps.end(), sprops.sps_nalu().begin(), sprops.sps_nalu().end());
					onData(id, sps.data(), sps.size(), presentationTime);

					std::vector<uint8_t> pps;
					pps.insert(pps.end(), marker, marker+sizeof(marker));
					pps.insert(pps.end(), sprops.pps_nalu().begin(), sprops.pps_nalu().end());
					onData(id, pps.data(), pps.size(), presentationTime);
				}
				else
				{
					RTC_LOG(WARNING) << "Cannot decode SPS:" << sprop;
				}
			}
			success = true;
		}
		else if (strcmp(codec, "JPEG") == 0)
		{
			m_codec = codec;
			success = true;
		}
	}
	return success;
}


void breaknow();
void breaknow()
{
	RTC_LOG(INFO) << "breaknow";
}



bool RTSPVideoCapturer::onData(const char* id, unsigned char* buffer, ssize_t size, struct timeval presentationTime)
{
	int64_t ts = presentationTime.tv_sec;
	ts = ts*1000 + presentationTime.tv_usec/1000;
	// RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData size:" << size << " ts:" << ts;
	int res = 0;
	bytesReceived += size;


	if (m_codec == "H264") {
		int nal_start = 0;
		int nal_end   = 0;
		find_nal_unit(buffer, size, &nal_start, &nal_end);
		read_nal_unit(m_h264, &buffer[nal_start], nal_end - nal_start);
		m_prevType = m_h264->nal->nal_unit_type;

		switch(m_h264->nal->nal_unit_type)
		{
			case NAL_UNIT_TYPE_SPS:
			{
				// RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData SPS";
				m_cfg.clear();
				m_cfg.insert(m_cfg.end(), buffer, buffer+size);

				unsigned int width = ((m_h264->sps->pic_width_in_mbs_minus1 +1)*16) - m_h264->sps->frame_crop_left_offset*2 - m_h264->sps->frame_crop_right_offset*2;
				unsigned int height= ((2 - m_h264->sps->frame_mbs_only_flag)* (m_h264->sps->pic_height_in_map_units_minus1 +1) * 16) - (m_h264->sps->frame_crop_top_offset * 2) - (m_h264->sps->frame_crop_bottom_offset * 2);
				//RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData SPS set timing_info_present_flag:" << m_h264->sps->vui.timing_info_present_flag << " " << m_h264->sps->vui.time_scale << " " << m_h264->sps->vui.num_units_in_tick;
				if (m_decoder.get()) {
					if ( ((unsigned int) GetCaptureFormat()->width != width) || ((unsigned int) GetCaptureFormat()->height != height) )  {
						RTC_LOG(INFO) << "format changed => set format from " << GetCaptureFormat()->width << "x" << GetCaptureFormat()->height	 << " to " << width << "x" << height;
						m_decoder.reset(NULL);
						RTC_LOG(INFO) << "RTSPVideoCapturer:onData resetting decoder.";

					}
				}

				if (!m_decoder.get()) {
					//RTC_LOG(INFO) << "RTSPVideoCapturer:onData SPS set format " << width << "x" << height << " fps:" << fps;
					cricket::VideoFormat videoFormat(width, height, cricket::VideoFormat::FpsToInterval(fps), cricket::FOURCC_I420);
					SetCaptureFormat(&videoFormat);

					m_decoder=m_factory.CreateVideoDecoder(webrtc::SdpVideoFormat(cricket::kH264CodecName));
					webrtc::VideoCodec codec_settings;
					codec_settings.codecType = webrtc::VideoCodecType::kVideoCodecH264;
					m_decoder->InitDecode(&codec_settings,2);
					m_decoder->RegisterDecodeCompleteCallback(this);
				}
			}
				break;
			case NAL_UNIT_TYPE_PPS:
				// RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData PPS";
				m_cfg.insert(m_cfg.end(), buffer, buffer+size);
				break;



			case NAL_UNIT_TYPE_CODED_SLICE_IDR:
				if (m_decoder.get()) {
					uint8_t buf[m_cfg.size() + size];
					memcpy(buf, m_cfg.data(), m_cfg.size());
					memcpy(buf+m_cfg.size(), buffer, size);
					webrtc::EncodedImage input_image(buf, sizeof(buf), sizeof(buf) + webrtc::EncodedImage::GetBufferPaddingBytes(webrtc::VideoCodecType::kVideoCodecH264));
					input_image._timeStamp = ts*1000;
					//RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData IDR ts="<<(ts*1000);
					res = m_decoder->Decode(input_image, false, NULL);
					if (res!=0)
					{
						RTC_LOG(INFO) << "RTSPVideoCapturer:onData decode failed NAL_UNIT_TYPE_CODED_SLICE_IDR :" << m_h264->nal->nal_unit_type;

					}

				} else 	{
					//RTC_LOG(LS_ERROR) << "RTSPVideoCapturer:onData no decoder";
					res = -1;
				}
				break;

			case NAL_UNIT_TYPE_AUD:
			case NAL_UNIT_TYPE_SEI:
				// these two nal units were causing warnings downstream in the webrtc Decoder code. Safe to ignore?
				// fallthrough.

			default:
				if (m_decoder.get()) {
					webrtc::EncodedImage input_image(buffer, size, size + webrtc::EncodedImage::GetBufferPaddingBytes(webrtc::VideoCodecType::kVideoCodecH264));
					input_image._timeStamp = ts*1000;
					//RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer:onData SLICE NALU:" << m_h264->nal->nal_unit_type << " ts=" << input_image._timeStamp;
					res = m_decoder->Decode(input_image, false, NULL);
					#if 1
					if (res!=0)
					{

						RTC_LOG(INFO) << "RTSPVideoCapturer:onData default failed nal=" << m_h264->nal->nal_unit_type << " m_prevType=" << m_prevType<<" size="<<size;


						//std::raise(SIGINT);
						#if 0
						breaknow();
						int retry = m_decoder->Decode(input_image, false, NULL);
						RTC_LOG(INFO) << "RTSPVideoCapturer:onData decode failed NALU:" << m_h264->nal->nal_unit_type << " res="<<res<<" m_prevType=" << m_prevType << " retry ="<<retry;
						breaknow();
						#endif

					}
					else
					{
							if (badPackets<50)
								RTC_LOG(INFO) << "RTSPVideoCapturer:onData default successNALU:" << m_h264->nal->nal_unit_type << " m_prevType=" << m_prevType;


					}
					#endif

				}else 	{
					RTC_LOG(LS_ERROR) << "RTSPVideoCapturer:onData no decoder";
					res = -1;
				}

		}

	} else if (m_codec == "JPEG") {
		int32_t width = 0;
		int32_t height = 0;
		if (libyuv::MJPGSize(buffer, size, &width, &height) == 0) {
			int stride_y = width;
			int stride_uv = (width + 1) / 2;

			rtc::scoped_refptr<webrtc::I420Buffer> I420buffer = webrtc::I420Buffer::Create(width, height, stride_y, stride_uv, stride_uv);
			const int conversionResult = libyuv::ConvertToI420((const uint8*)buffer, size,
							(uint8*)I420buffer->DataY(), I420buffer->StrideY(),
							(uint8*)I420buffer->DataU(), I420buffer->StrideU(),
							(uint8*)I420buffer->DataV(), I420buffer->StrideV(),
							0, 0,
							width, height,
							width, height,
							libyuv::kRotate0, ::libyuv::FOURCC_MJPG);

			if (conversionResult >= 0) {
				webrtc::VideoFrame frame(I420buffer, 0, ts*1000, webrtc::kVideoRotation_0);
				this->Decoded(frame);
			} else {
				RTC_LOG(LS_ERROR) << "RTSPVideoCapturer:onData decoder error:" << conversionResult;
				res = -1;
			}
		} else {
			RTC_LOG(LS_ERROR) << "RTSPVideoCapturer:onData cannot determine JPEG dimension";
			res = -1;
		}

	}
	if (res==0)
	{
		goodPackets++;
	}
	else
	{
		badPackets++;
		if (badPackets <100 && (badPackets % 100==0) )
		RTC_LOG(INFO) << "onData BHL bytesReceived=" << bytesReceived<<" goodPackets="<<goodPackets<<" badPackets="<<badPackets;
	}



	return (res == 0);
}

ssize_t RTSPVideoCapturer::onNewBuffer(unsigned char* buffer, ssize_t size)
{
	ssize_t markerSize = 0;
	if (m_codec == "H264") {
		if (size > (ssize_t) sizeof(marker))
		{
			memcpy( buffer, marker, sizeof(marker) );
			markerSize = sizeof(marker);
		}
	}
	return 	markerSize;
}

int32_t RTSPVideoCapturer::Decoded(webrtc::VideoFrame& decodedImage)
{
	decodedFrames++;
	if (decodedImage.timestamp_us() == 0) {
		decodedImage.set_timestamp_us(decodedImage.timestamp());
	}
	RTC_LOG(LS_VERBOSE) << "RTSPVideoCapturer::Decoded " << decodedImage.size() << " " << decodedImage.timestamp_us() << " " << decodedImage.timestamp() << " " << decodedImage.ntp_time_ms() << " " << decodedImage.render_time_ms();
	this->OnFrame(decodedImage, decodedImage.height(), decodedImage.width());
	return true;
}

cricket::CaptureState RTSPVideoCapturer::Start(const cricket::VideoFormat& format)
{
	SetCaptureFormat(&format);
	SetCaptureState(cricket::CS_RUNNING);
	rtc::Thread::Start();
	return cricket::CS_RUNNING;
}

void RTSPVideoCapturer::Stop()
{
	m_env.stop();
	rtc::Thread::Stop();
	SetCaptureFormat(NULL);
	SetCaptureState(cricket::CS_STOPPED);
	RTC_LOG(INFO) << "RTSPVideoCapturer::Stop";
}

void RTSPVideoCapturer::Run()
{
	m_env.mainloop();
}

bool RTSPVideoCapturer::GetPreferredFourccs(std::vector<unsigned int>* fourccs)
{
	return true;
}
#endif
