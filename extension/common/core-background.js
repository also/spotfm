spotfm.call = function (action, options) {
    if (spotfm.currentPage) {
        extension.send(spotfm.currentPage, {action: action, options: options});
    }
};

spotfm.setPlaylist = function (playlist, playlistOffset) {
    spotfm.playlist = playlist;
    spotfm.playlistOffset = playlistOffset || 0;
};

spotfm.playOffset = function (offset) {
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
};

spotfm.setCurrentPage = function (page) {
    // TODO notify current page
    spotfm.currentPage = page;
};

spotfm.playFromPlaylist = function (page, playlist, offset) {
    spotfm.setCurrentPage(page);
    spotfm.setPlaylist(playlist);
    spotfm.playOffset(offset);
};

spotfm.next = function() {
    if (spotfm.playlist && spotfm.playlistOffset + 1 < spotfm.playlist.length) {
        spotfm.playOffset(spotfm.playlistOffset + 1);
    }
};

spotfm.previous = function() {
    if (spotfm.playlist && spotfm.playlistOffset - 1 >= 0) {
        spotfm.playOffset(spotfm.playlistOffset - 1);
    }
};

spotfm.tryToPlay = function () {
    if (spotfm.playlist) {
        spotfm.playOffset(spotfm.playlistOffset);
    }
};

spotfm.onTrackStart = function () {
    spotfm.scrobbled = false;
    if (spotfm.currentTrack && spotfm.currentTrack.lastfmId) {
        spotfm.nowplaying(spotfm.currentTrack.lastfmId);
    }
};

spotfm.onPositionChange = function (position) {
    if (spotfm.scrobbled || !spotfm.currentTrack || !spotfm.player.duration || spotfm.player.duration < 30000) {
        return;
    }
    if (position >= Math.min(4 * 60 * 1000, spotfm.player.duration / 2)) {
        spotfm.scrobbled = true;
        spotfm.scrobble(spotfm.currentTrack.lastfmId);
    }
};

spotfm.scrobble = function (lastfmTrackId) {
    $.ajax({url: 'http://www.last.fm/ajax/scrobble', type:'post', data: 'track=' + lastfmTrackId + '&formtoken=' + spotfm.lastfmSession.formtoken});
};

spotfm.nowplaying = function (lastfmTrackId) {
    $.ajax({url: 'http://www.last.fm/ajax/nowplaying', type:'post', data: 'track=' + lastfmTrackId + '&formtoken=' + spotfm.lastfmSession.formtoken});
};

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

spotfm.connect();
