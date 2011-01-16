var TERRITORY = 'SE';

_.extend(spotify, {
    generateQuery: function(info) {
        var parts = [];
        ['artist', 'album', 'track'].forEach(function (type) {
            // TODO escape quotes
            if (info[type]) parts.push(type + ':"' + info[type] + '"');
        });
        return parts.join(' ');
    },

    generateTrackQuery: function (trackInfo) {
        // TODO escape quotes
        return 'artist:"' + trackInfo.artist + '" track:"' + trackInfo.track + '"';
    },

    resolve: function (trackInfo, options) {
        options = options || {};
        var query = escape(this.generateTrackQuery(trackInfo));
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

    openTrackInApp: function (trackInfo) {
        omnifm.resolve(trackInfo, {
            onResolution: function (id) {
                extension.openExternalUri(id);
            },
            onResolutionFailure: function () {
                extension.openExternalUri('spotify:search:' + spotify.generateTrackQuery(trackInfo));
            }
        });
    },

    searchTrackInApp: function (trackInfo) {
        extension.openExternalUri('spotify:search:' + spotify.generateTrackQuery(trackInfo));
    },

    searchObjectInApp: function (info) {
        extension.openExternalUri('spotify:search:' + spotify.generateQuery(info));
    }
});
