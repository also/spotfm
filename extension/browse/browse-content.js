function xm(action) {
    return function (options) {
        omnifm.call(action, options);
    }
}

var omnifm = {
    callbackNum: 0,
    callbacks: {},

    call: function (action, options) {
        extension.send({action: action, options: options});
    },

    resolve: function (trackInfo, options) {
        var callbackNum = '' + omnifm.callbackNum++;
        omnifm.callbacks[callbackNum] = options;
        omnifm.call('resolve', {trackInfo: trackInfo, context: callbackNum});
    },

    resolveAndPlay: function (trackInfo) {
        omnifm.call('resolveAndPlay', {trackInfo: trackInfo});
    },

    playFromPlaylist: function (playlist, offset) {
        omnifm.call('playFromPlaylist', {playlist: playlist, offset: offset});
    },

    setLastfmSession: function (lastfmSession) {
        omnifm.call('setLastfmSession', lastfmSession);
    },

    pageUnloaded: function () {
        omnifm.call('pageUnloaded');
    }
};

$(window).bind('unload', function (event) {
    omnifm.pageUnloaded();
});

var trackDetailIframe;

function showDetail(event) {
    trackDetailIframe.css({top: event.pageY + 'px', left: event.pageX + 'px'});
    trackDetailIframe.addClass('loading');
    trackDetailIframe.show();

    var frameBody = trackDetailIframe.get(0).contentDocument.body;
    $('.track-detail', frameBody).text('resolving...');

    var trackInfo = getRowInfo($(event.target).closest('tr').get(0));
    omnifm.resolve(trackInfo, {
        onResolution: function (spotifyId) {
            $.ajax({url: 'http://localhost:9999/track_detail/' + spotifyId, method:'get',
                success: function (o) {
                    trackDetailIframe.removeClass('loading');
                    var img = $('<img class="album-cover" src="data:image/jpeg;base64,' + o.album_cover + '"/>');
                    $('.track-detail', frameBody).empty().append(img);
                }
            });
        }
    });
}

var currentTrElt;
var currentPlaylist;
var currentPlaylistOffset = null;

extension.onMessage = function(message) {
    var action = message.action;
    var options = message.options;

    if (action == 'callback') {
        var callbackOptions = omnifm.callbacks[options.context];
        var callback = callbackOptions[options.callback];
        if (callback) {
            callback.apply(callbackOptions, options.args);
        }
    }
    if (action == 'onResolution') {
        onResolution(options.offset, options.id);
    }
    else if (action == 'onResolutionFailure') {
        onResolutionFailure(options.offset);
    }
    else if (action == 'onPlay') {
        onPlay(options.offset);
    }
};

function onResolution(offset, id) {
    $(currentPlaylist.trElts[offset]).attr('data-spotify-id', id);
}

function onResolutionFailure(offset) {
    var tr = $(currentPlaylist.trElts[offset]);
    // TODO without adding a class, the data attribute doesn't seem
    // to be noticed until the mouse moves out of the row
    tr.addClass('thunk');
    tr.attr('data-spotify-unresolvable', 'true');
}

function onPlay(offset)  {
    if (currentPlaylistOffset != null) {
        $(currentPlaylist.trElts[currentPlaylistOffset]).removeClass('omnifmPlaying');
    }

    $(currentPlaylist.trElts[offset]).addClass('omnifmPlaying');
    currentPlaylistOffset = offset;
}

function playTrElt(trElt) {
    var trElts = $(trElt).closest('tbody').children('tr').get();

    var tracks = trElts.map(getRowInfo);

    omnifm.playFromPlaylist(tracks, trElts.indexOf(trElt));

    currentPlaylist = {
        tracks: tracks,
        trElts: trElts
    }
}

$(function () {
    trackDetailIframe = $('<iframe class="track-detail-frame"/>');
    trackDetailIframe.hide();
    $(document.body).append(trackDetailIframe);
    var frameBody = trackDetailIframe.get(0).contentDocument.body;
    $(frameBody).append('<link rel="stylesheet" href="' + extension.getURL('detail-frame.css') + '"/><div class="track-detail"></div>');
});
