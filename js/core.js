var spotfm = {
    play: function (trackId) {
        var audioRequest = new Request({url: "http://localhost:9999/play/" + trackId, method:'get'});
        audioRequest.send();
    },

    stop: function () {
        new Request({url: 'http://localhost:9999/stop'}).send();
    },

    resume: function () {
        new Request({url: 'http://localhost:9999/resume'}).send();
    },

    resolveAndPlay: function (trackInfo, options) {
        options = options || {};
        var query = [escape(trackInfo.artist), escape(trackInfo.track)].join('+');
            var requestURL = 'http://ws.spotify.com/search/1/track.json?q=' + query;
            console.log(requestURL);
            var jsonRequest = new Request.JSON({url: requestURL, method:'get', headers: {}, onSuccess: function(json_object, json_string){
                var tracks = json_object.tracks;
                if (tracks.length !== 0) {
                    var track = tracks[0];
                    var id = track.href;
                    console.log([track.artists[0].name, track.album.name, track.name].join(' '));
                    spotfm.play(id);
                }
                else {
                    if (options.onResolutionFailure) {
                        options.onResolutionFailure();
                    }
                }
            }
        });
        jsonRequest.send();
    }
};
