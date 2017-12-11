/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** HttpServerHandler.h
**
** -------------------------------------------------------------------------*/

#include "CivetServer.h"
#include "PeerConnectionManager.h"

typedef std::function<Json::Value(const struct mg_request_info *, const Json::Value &)> httpFunction;

/* ---------------------------------------------------------------------------
**  http callback
** -------------------------------------------------------------------------*/
class HttpServerRequestHandler : public CivetServer
{
	public:
		HttpServerRequestHandler(PeerConnectionManager* webRtcServer, const std::vector<std::string>& options);

		httpFunction getFunction(const std::string& uri);


	protected:

		std::string prefix;		// server prefix. Either "" or something like "/webrtc-api"
		std::string auth_key;		// admin auth_key to allow managing tokens/streams.

		PeerConnectionManager* m_webRtcServer;
		std::map<std::string,httpFunction> m_func;
		bool isAdmin(const struct mg_request_info *req_info, const Json::Value & in);

		bool hasToken(const struct mg_request_info *req_info, const Json::Value & in);	// check if user has supplied a valid token.

		std::string getParam(const struct mg_request_info *req_info, const Json::Value & in, const char *arg);

		const Json::Value unauthorized() {
			std::string msg("unauthorized");
			return PeerConnectionManager::error(msg);
		}


};
