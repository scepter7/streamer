RTCPeerConnection = window.RTCPeerConnection || window.mozRTCPeerConnection || window.webkitRTCPeerConnection;
RTCSessionDescription = window.RTCSessionDescription || window.mozRTCSessionDescription || window.webkitRTCSessionDescription;
RTCIceCandidate = window.RTCIceCandidate || window.mozRTCIceCandidate || window.webkitRTCIceCandidate;
URL = window.URL || window.webkitURL;


/**
 * Interface with WebRTC-streamer API
 * @constructor
 * @param {string} videoElement - id of the video element tag
 * @param {string} srvurl -  url of webrtc-streamer (default is current location)
 */
function WebRtcStreamer(videoElement, srvurl, token, l) {
  this.videoElement = videoElement;
  this.srvurl = srvurl || location.protocol + "//" + window.location.hostname + ":" + window.location.port;
  this.pc = null;
  this.token = token;
  this.listener = l;

  this.pcOptions = {
    'optional': [{
      'DtlsSrtpKeyAgreement': true
    }]
  };

  this.mediaConstraints = {}
  if (navigator.userAgent.indexOf("Firefox") > 0) {
    this.mediaConstraints = {
      'offerToReceiveVideo': true,
      'offerToReceiveAudio': true
    };
  } else {
    this.mediaConstraints = {
      'mandatory': {
        'OfferToReceiveVideo': true,
        'OfferToReceiveAudio': true
      }
    }
  }

  this.iceServers = null;
}

/**
 * Connect a WebRTC Stream to videoElement
 * @param {string} streamID - id of stream
 */
WebRtcStreamer.prototype.connect = function(streamID) {

  // getIceServers is not already received

  console.log("connect: " + streamID + " Get IceServers. token=" + this.token);
  send(this.srvurl + "/getIceServers?token=" + this.token + "&id=" + streamID, null, null, function(iceServers) {
    this.onReceiveGetIceServers(iceServers, streamID);
  }, null, this);

}

/*
 *
 */
WebRtcStreamer.prototype.onReceiveGetIceServers = function(iceServers, streamID) {
  this.iceServers = iceServers;
  this.pcConfig = iceServers || {
    'iceServers': []
  };
  try {
    this.pc = this.createPeerConnection();

    var peerid = this.token ? this.token : Math.random().toString(36).substring(7); //  Math.random();
    this.pc.peerid = peerid;

    var streamer = this;
    var callurl = this.srvurl + "/call?peerid=" + peerid + "&id=" + streamID;

    if (this.token)
      callurl += "&token=" + this.token

    // create Offer
    this.pc.createOffer(function(sessionDescription) {
      console.log("Create offer:" + JSON.stringify(sessionDescription));

      streamer.pc.setLocalDescription(sessionDescription, function() {
        send(callurl, null, sessionDescription, streamer.onReceiveCall, null, streamer);
      }, function() {});

    }, function(error) {
      alert("Create offer error:" + JSON.stringify(error));
    }, this.mediaConstraints);

  } catch (e) {
    this.disconnect();
    alert("connect error: " + e);
  }
}

/**
 * Disconnect a WebRTC Stream and clear videoElement source
 */
WebRtcStreamer.prototype.disconnect = function() {
  var videoElement = document.getElementById(this.videoElement);
  if (videoElement) {
    //	videoElement.src = ''
    videoElement.parentElement.removeChild(videoElement);

  } else {
    console.log("disconnect didn't find videoElement.");
  }
  if (this.pc) {
    send(this.srvurl + "/hangup?peerid=" + this.pc.peerid);
    try {
      this.pc.close();
    } catch (e) {
      console.log("Failure close peer connection:" + e);
    }
    this.listener.error("disconnect called");
    this.pc = null;
  }
}

/*
 * create RTCPeerConnection
 */
WebRtcStreamer.prototype.createPeerConnection = function() {
  console.log("createPeerConnection  config: " + JSON.stringify(this.pcConfig) + " option:" + JSON.stringify(this.pcOptions));
  var pc = new RTCPeerConnection(this.pcConfig, this.pcOptions);
  var streamer = this;
  pc.onicecandidate = function(evt) {
    streamer.onIceCandidate.call(streamer, evt)
  };
  if (typeof pc.ontrack != "undefined") {
    pc.ontrack = function(evt) {
      streamer.onTrack.call(streamer, evt)
    };
  } else {
    pc.onaddstream = function(evt) {
      streamer.onTrack.call(streamer, evt)
    };
  }
  pc.oniceconnectionstatechange = function(evt) {
    console.log("oniceconnectionstatechange  state: " + pc.iceConnectionState);
    var videoElement = document.getElementById(streamer.videoElement);
    if (videoElement) {
      if (pc.iceConnectionState == "connected") {
        videoElement.style.opacity = "1.0";
        streamer.listener.connected(pc.iceConnectionState);

      } else if (pc.iceConnectionState == "disconnected") {
        streamer.listener.error(pc.iceConnectionState);
      } else if ((pc.iceConnectionState == "failed") || (pc.iceConnectionState == "closed")) {
        streamer.listener.error(pc.iceConnectionState);

      }
    }
  }
  pc.ondatachannel = function(evt) {
    console.log("remote datachannel created:" + JSON.stringify(evt));

    evt.channel.onopen = function() {
      console.log("remote datachannel open");
      this.send("remote channel openned");
    }
    evt.channel.onmessage = function(event) {
      console.log("remote datachannel recv:" + JSON.stringify(event.data));
    }
  }

  var dataChannel = pc.createDataChannel("ClientDataChannel");
  dataChannel.onopen = function() {
    console.log("local datachannel open");
    this.send("local channel openned");
  }
  dataChannel.onmessage = function(evt) {
    console.log("local datachannel recv:" + JSON.stringify(evt.data));
  }

  console.log("Created RTCPeerConnnection with config: " + JSON.stringify(this.pcConfig) + "option:" + JSON.stringify(this.pcOptions));
  return pc;
}

/*
 * RTCPeerConnection IceCandidate callback
 */
WebRtcStreamer.prototype.onIceCandidate = function(event) {
  if (event.candidate) {
    send(this.srvurl + "/addIceCandidate?peerid=" + this.pc.peerid + "&token=" + this.token, null, event.candidate);
  } else {
    console.log("End of candidates.");
  }
}

/*
 * RTCPeerConnection AddTrack callback
 */
WebRtcStreamer.prototype.onTrack = function(event) {
  console.log("Remote track added:" + JSON.stringify(event));
  if (event.streams) {
    stream = event.streams[0];
  } else {
    stream = event.stream;
  }
  var videoElement = document.getElementById(this.videoElement);
  videoElement.src = URL.createObjectURL(stream);
  videoElement.play();
}

/*
 * AJAX /call callback
 */
WebRtcStreamer.prototype.onReceiveCall = function(dataJson) {
  var that = this;
  console.log("offer: " + JSON.stringify(dataJson));

  if (!this.pc) {
    console.log("pc closed");
    return;
  }
  if (dataJson.error) {
    that.listener.error("onReceiveCall: " + dataJson.error);
    return;
  }

  var peerid = this.pc.peerid;
  this.pc.setRemoteDescription(new RTCSessionDescription(dataJson), function() {
    console.log("setRemoteDescription ok")
    send(that.srvurl + "/getIceCandidate?peerid=" + that.pc.peerid + "&token=" + that.token, null, null, that.onReceiveCandidate, null, that);
  }, function(error) {
    console.log("setRemoteDescription error:" + JSON.stringify(error));
  });
}

/*
 * AJAX /getIceCandidate callback
 */
WebRtcStreamer.prototype.onReceiveCandidate = function(dataJson) {
  console.log("candidate: " + JSON.stringify(dataJson));
  if (dataJson) {
    for (var i = 0; i < dataJson.length; i++) {
      var candidate = new RTCIceCandidate(dataJson[i]);

      console.log("Adding ICE candidate :" + JSON.stringify(candidate));
      this.pc.addIceCandidate(candidate, function() {
        console.log("addIceCandidate OK");
      }, function(error) {
        console.log("addIceCandidate error:" + JSON.stringify(error));
      });
    }
  }
}
