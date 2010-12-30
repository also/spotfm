var TERRITORY = 'SE';

var spotfm = {
    player: {position: 0},

    play: function (trackId) {
        $.get('http://localhost:9999/play/' + trackId);
    },

    pause: function () {
        $.get('http://localhost:9999/stop');
    },

    resume: function () {
        $.get('http://localhost:9999/resume');
    },

    resolve: function (trackInfo, options) {
        options = options || {};
        var query = [escape(trackInfo.artist), escape(trackInfo.track)].join('+');
        var requestURL = 'http://ws.spotify.com/search/1/track.json?q=' + query;
        $.ajax({url: requestURL, method:'get', success: function (json_object) {
            var tracks = json_object.tracks.filter(function (track) {
                return track.album.availability.territories.indexOf(TERRITORY) >= 0;
            });
            if (tracks.length !== 0) {
                var track = tracks[0];
                var id = track.href;
                if (options.onResolution) {
                    options.onResolution(id);
                }
            }
            else {
                if (options.onResolutionFailure) {
                    options.onResolutionFailure();
                }
            }
        }});
    },

    resolveAndPlay: function (trackInfo, options) {
        options = options || {};
        var onResolution = options.onResolution;
        options.onResolution = function(trackId) {
            if (onResolution) {
                onResolution(trackId);
            }
            spotfm.play(trackId);
        }
        spotfm.resolve(trackInfo, options);
    },

    connect: function () {
        if (spotfm.ws) {
            return;
        }
        console.log('trying to connect');
        var ws = new WebSocket('ws://localhost:9999/monitor');
        spotfm.ws = ws;
        ws.onopen = function () {
            console.log('open');
        };

        ws.onerror = function () {
            spotfm.ws = null;
            console.log('spotfm connection error');
            window.setTimeout(spotfm.connect, 1000);
        };

        ws.onclose = function () {
            spotfm.ws = null;
            console.log('spotfm connection closed');
            window.setTimeout(spotfm.connect, 1000);
        };

        ws.onmessage = function (a) {
            if (a.data.length > 0) {
                var j = JSON.parse(a.data);
                if (j.event == 'track_info') {
                    spotfm.player.trackInfo = j;
                    spotfm.player.duration = j.track_duration;
                    if (spotfm.onTrackInfo) {
                        spotfm.onTrackInfo(j);
                    }
                }
                else if (j.event == 'start_of_track') {
                    if (spotfm.onTrackStart) {
                        spotfm.onTrackStart();
                    }
                }
                else if (j.event == 'playing') {
                    spotfm.setPosition(spotfm.player.position);
                    spotfm.positionUpdateInterval = window.setInterval(spotfm.updatePosition, 500);
                    if (spotfm.onPlay) {
                        spotfm.onPlay();
                    }
                }
                else if (j.event == 'stopped') {
                    window.clearInterval(spotfm.positionUpdateInterval);
                    if (spotfm.onStop) {
                        spotfm.onStop();
                    }
                }
                else if (j.event == 'position') {
                    spotfm.setPosition(j.position);
                }
                else if (j.event == 'next' || j.event == 'end_of_track' || j.event == 'playback_failed') {
                    if (spotfm.next) {
                        spotfm.next();
                    }
                }
                else if (j.event == 'previous') {
                    if (spotfm.previous()) {
                        spotfm.previous();
                    }
                }
                else if (j.event == 'play') {
                    if (spotfm.tryToPlay) {
                        spotfm.tryToPlay();
                    }
                }
            }
        };
    },

    setPosition: function (position) {
        spotfm.player.position = position;
        spotfm.timeOffset = new Date().getTime() - position;
        if (spotfm.onPositionChange) {
            spotfm.onPositionChange(position);
        }
    },

    updatePosition: function () {
        var now = new Date().getTime();
        var played = now - spotfm.timeOffset;
        if (played >= spotfm.player.duration) {
            window.clearInterval(spotfm.positionUpdateInterval);
            played = spotfm.player.duration;
        }
        if (spotfm.onPositionChange) {
            spotfm.onPositionChange(spotfm.player.position);
        }
    }
};
