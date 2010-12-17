function xm(action) {
    return function (options) {
        spotfm.call(action, options);
    }
}

var spotfm = {
    callbackNum: 0,
    callbacks: {},

    call: function (action, options) {
        extension.send({action: action, options: options});
    },

    resolve: function (trackInfo, options) {
        var callbackNum = '' + spotfm.callbackNum++;
        spotfm.callbacks[callbackNum] = options;
        spotfm.call('resolve', {trackInfo: trackInfo, context: callbackNum});
    },

    resolveAndPlay: function (trackInfo) {
        spotfm.call('resolveAndPlay', {trackInfo: trackInfo});
    },

    playFromPlaylist: function (playlist, offset) {
        spotfm.call('playFromPlaylist', {playlist: playlist, offset: offset});
    },

    setLastfmSession: function (lastfmSession) {
        spotfm.call('setLastfmSession', lastfmSession);
    },

    pageUnloaded: function () {
        spotfm.call('pageUnloaded');
    }
};
