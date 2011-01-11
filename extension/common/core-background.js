_.extend(spotfm, {
    call: function (action, options) {
        if (spotfm.currentPage) {
            extension.send(spotfm.currentPage, {action: action, options: options});
        }
    },

    setPlaylist: function (playlist, playlistOffset) {
        spotfm.playlist = playlist;
        spotfm.playlistOffset = playlistOffset || 0;
    },

    playOffset: function (offset) {
        var trackInfo = spotfm.playlist[offset];

        spotfm.currentTrack = trackInfo;
        spotfm.playlistOffset = offset;

        spotfm.call('onPlay', {offset: offset});

        if (trackInfo.spotifyId) {
            spotfm.play(trackInfo.spotifyId);
        }
        else if (trackInfo.unresolvable) {
            spotfm.next();
        }
        else {
            spotfm.resolveAndPlay(trackInfo, {
                onResolution: function (id) {
                    trackInfo.spotifyId = id;
                    spotfm.call('onResolution', {offset: offset, id: id});
                },
                onResolutionFailure: function () {
                    trackInfo.unresolvable = null;
                    spotfm.call('onResolutionFailure', {offset: offset});
                    spotfm.next();
                }
            });
        }
    },

    setCurrentPage: function (page) {
        // TODO notify current page
        spotfm.currentPage = page;
    },

    playFromPlaylist: function (page, playlist, offset) {
        spotfm.setCurrentPage(page);
        spotfm.setPlaylist(playlist);
        spotfm.playOffset(offset);
    },

    next: function() {
        if (spotfm.playlist && spotfm.playlistOffset + 1 < spotfm.playlist.length) {
            spotfm.playOffset(spotfm.playlistOffset + 1);
        }
    },

    previous: function() {
        if (spotfm.playlist && spotfm.playlistOffset - 1 >= 0) {
            spotfm.playOffset(spotfm.playlistOffset - 1);
        }
    },

    tryToPlay: function () {
        if (spotfm.playlist) {
            spotfm.playOffset(spotfm.playlistOffset);
        }
    },

    onTrackStart: function () {
        spotfm.scrobbled = false;
        if (extension.getSetting('scrobble') && spotfm.currentTrack && spotfm.currentTrack.lastfmId) {
            spotfm.nowplaying(spotfm.currentTrack.lastfmId);
        }
    },

    onPositionChange: function (position) {
        // only scrobble if
        //  * scrobbling is enabled in settings
        //  * we haven't already scrobbled
        //  * there is a current track
        //  * the track isn't empty
        //  * the track is longer than 30 seconds (last.fm rule)
        if (!extension.getSetting('scrobble') || spotfm.scrobbled || !spotfm.currentTrack || !spotfm.player.duration || spotfm.player.duration < 30000) {
            return;
        }
        //  * the player is 4 minutes or halfway through the track, whichever is earlier (last.fm rule)
        if (position >= Math.min(4 * 60 * 1000, spotfm.player.duration / 2)) {
            spotfm.scrobbled = true;
            spotfm.scrobble(spotfm.currentTrack.lastfmId);
        }
    },

    scrobble: function (lastfmTrackId) {
        $.ajax({url: 'http://www.last.fm/ajax/scrobble', type:'post', data: 'track=' + lastfmTrackId + '&formtoken=' + spotfm.lastfmSession.formtoken});
    },

    nowplaying: function (lastfmTrackId) {
        $.ajax({url: 'http://www.last.fm/ajax/nowplaying', type:'post', data: 'track=' + lastfmTrackId + '&formtoken=' + spotfm.lastfmSession.formtoken});
    }
});

extension.onMessage = function (sender, message) {
    var action = message.action;
    var options = message.options;
    if (action == 'resolve') {
        spotfm.resolve(options.trackInfo, {
            onResolution: function() {
                extension.send(sender, {action: 'callback', options: {context: options.context, callback: 'onResolution', args: Array.prototype.slice.call(arguments)}});
            }
        });
    }
    if (action == 'resolveAndPlay') {
        spotfm.resolveAndPlay(options.trackInfo);
    }
    else if (action == 'playFromPlaylist') {
        spotfm.playFromPlaylist(sender, options.playlist, options.offset);
    }
    else if (action == 'setLastfmSession') {
        spotfm.lastfmSession = options;
    }
    else if (action == 'pageUnloaded') {
        if (spotfm.currentPage == sender) {
            spotfm.setCurrentPage(null);
        }
    }
};

_.extend(spotify, {
    openTrackInApp: function (trackInfo) {
        spotfm.resolve(trackInfo, {
            onResolution: function (id) {
                extension.openExternalUri(id);
            },
            onResolutionFailure: function () {
                extension.openExternalUri('spotify:search:' + spotfm.generateTrackQuery(trackInfo));
            }
        });
    },

    searchTrackInApp: function (trackInfo) {
        extension.openExternalUri('spotify:search:' + spotfm.generateTrackQuery(trackInfo));
    },

    searchObjectInApp: function (info) {
        extension.openExternalUri('spotify:search:' + spotfm.generateQuery(info));
    }
});

if (window.safari) {
    safari.application.addEventListener('contextmenu', function (event) {
        if (event.userInfo.objectInfo) {//} && (!event.userInfo.trackInfo || event.userInfo.objectInfo.type != 'track')) {
            event.contextMenu.appendContextMenuItem('searchObjectInApp', 'Search in Spotify');
        }
        if (event.userInfo.trackInfo) {
            event.contextMenu.appendContextMenuItem('searchTrackInApp', 'Search Track in Spotify');
            event.contextMenu.appendContextMenuItem('openTrackInApp', 'Open Track in Spotify');
        }
    }, false);

    safari.application.addEventListener('command', function (event) {
        if (event.command == 'searchTrackInApp') {
            spotify.searchTrackInApp(event.userInfo.trackInfo);
        }
        else if (event.command == 'openTrackInApp') {
            spotify.openTrackInApp(event.userInfo.trackInfo)
        }
        else if (event.command == 'searchObjectInApp') {
            spotify.searchObjectInApp(event.userInfo.objectInfo);
        }
    }, false);
}

spotfm.source = spotify;

spotfm.connect();
