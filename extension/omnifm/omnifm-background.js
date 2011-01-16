_.extend(omnifm, {
    call: function (action, options) {
        if (omnifm.currentPage) {
            extension.send(omnifm.currentPage, {action: action, options: options});
        }
    },

    setPlaylist: function (playlist, playlistOffset) {
        omnifm.playlist = playlist;
        omnifm.playlistOffset = playlistOffset || 0;
    },

    playOffset: function (offset) {
        var trackInfo = omnifm.playlist[offset];

        omnifm.currentTrack = trackInfo;
        omnifm.playlistOffset = offset;

        omnifm.call('onPlay', {offset: offset});

        if (trackInfo.spotifyId) {
            omnifm.play(trackInfo.spotifyId);
        }
        else if (trackInfo.unresolvable) {
            omnifm.next();
        }
        else {
            omnifm.resolveAndPlay(trackInfo, {
                onResolution: function (id) {
                    trackInfo.spotifyId = id;
                    omnifm.call('onResolution', {offset: offset, id: id});
                },
                onResolutionFailure: function () {
                    trackInfo.unresolvable = null;
                    omnifm.call('onResolutionFailure', {offset: offset});
                    omnifm.next();
                }
            });
        }
    },

    setCurrentPage: function (page) {
        // TODO notify current page
        omnifm.currentPage = page;
    },

    playFromPlaylist: function (page, playlist, offset) {
        omnifm.setCurrentPage(page);
        omnifm.setPlaylist(playlist);
        omnifm.playOffset(offset);
    },

    next: function() {
        if (omnifm.playlist && omnifm.playlistOffset + 1 < omnifm.playlist.length) {
            omnifm.playOffset(omnifm.playlistOffset + 1);
        }
    },

    previous: function() {
        if (omnifm.playlist && omnifm.playlistOffset - 1 >= 0) {
            omnifm.playOffset(omnifm.playlistOffset - 1);
        }
    },

    tryToPlay: function () {
        if (omnifm.playlist) {
            omnifm.playOffset(omnifm.playlistOffset);
        }
    },

    onTrackStart: function () {
        omnifm.scrobbled = false;
        if (extension.getSetting('scrobble') && omnifm.currentTrack && omnifm.currentTrack.lastfmId) {
            omnifm.nowplaying(omnifm.currentTrack.lastfmId);
        }
    },

    onPositionChange: function (position) {
        // only scrobble if
        //  * scrobbling is enabled in settings
        //  * we haven't already scrobbled
        //  * there is a current track
        //  * the track isn't empty
        //  * the track is longer than 30 seconds (last.fm rule)
        if (!extension.getSetting('scrobble') || omnifm.scrobbled || !omnifm.currentTrack || !omnifm.player.duration || omnifm.player.duration < 30000) {
            return;
        }
        //  * the player is 4 minutes or halfway through the track, whichever is earlier (last.fm rule)
        if (position >= Math.min(4 * 60 * 1000, omnifm.player.duration / 2)) {
            omnifm.scrobbled = true;
            omnifm.scrobble(omnifm.currentTrack.lastfmId);
        }
    },

    scrobble: function (lastfmTrackId) {
        $.ajax({url: 'http://www.last.fm/ajax/scrobble', type:'post', data: 'track=' + lastfmTrackId + '&formtoken=' + omnifm.lastfmSession.formtoken});
    },

    nowplaying: function (lastfmTrackId) {
        $.ajax({url: 'http://www.last.fm/ajax/nowplaying', type:'post', data: 'track=' + lastfmTrackId + '&formtoken=' + omnifm.lastfmSession.formtoken});
    }
});

extension.onMessage = function (sender, message) {
    var action = message.action;
    var options = message.options;
    if (action == 'resolve') {
        omnifm.resolve(options.trackInfo, {
            onResolution: function() {
                extension.send(sender, {action: 'callback', options: {context: options.context, callback: 'onResolution', args: Array.prototype.slice.call(arguments)}});
            }
        });
    }
    if (action == 'resolveAndPlay') {
        omnifm.resolveAndPlay(options.trackInfo);
    }
    else if (action == 'playFromPlaylist') {
        omnifm.playFromPlaylist(sender, options.playlist, options.offset);
    }
    else if (action == 'setLastfmSession') {
        omnifm.lastfmSession = options;
    }
    else if (action == 'pageUnloaded') {
        if (omnifm.currentPage == sender) {
            omnifm.setCurrentPage(null);
        }
    }
    else if (action == 'sourceLoaded') {
        omnifm.sourceTab = sender;
    }
    else if (action == 'onNext') {
        omnifm.next();
    }
    else if (action == 'onPositionChange') {
        omnifm.setPosition(options);
    }
};

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
