var currentTrElt;
var currentPlaylist;
var currentPlaylistOffset = null;
var trackDetailIframe;

$('.candyStriped td, .tracklist td').live('click', function (event) {
    var target = event.target;
    if ($(target).closest('td.playbuttonCell').length > 0) {
        console.log('clicked spotify icon');
        showDetail(event);
        return;
    }
    var trElt = $(target).closest('tr').get(0);
    playTrElt(trElt);
});

function showDetail(event) {
    trackDetailIframe.css({top: event.pageY + 'px', left: event.pageX + 'px'});
    trackDetailIframe.addClass('loading');
    trackDetailIframe.show();

    var frameBody = trackDetailIframe.get(0).contentDocument.body;
    $('.track-detail', frameBody).text('resolving...');

    var trackInfo = getRowInfo($(event.target).closest('tr').get(0));
    spotfm.resolve(trackInfo, {
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

$(window).bind('unload', function (event) {
    spotfm.pageUnloaded();
});

extension.onMessage = function(message) {
    var action = message.action;
    var options = message.options;

    if (action == 'callback') {
        console.log('callback');
        var callbackOptions = spotfm.callbacks[options.context];
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
        $(currentPlaylist.trElts[currentPlaylistOffset]).removeClass('spotfmPlaying');
    }

    $(currentPlaylist.trElts[offset]).addClass('spotfmPlaying');
    currentPlaylistOffset = offset;
}

function playTrElt(trElt) {
    var trElts = $(trElt).closest('tbody').children('tr').get();

    var tracks = trElts.map(getRowInfo);

    spotfm.playFromPlaylist(tracks, trElts.indexOf(trElt));

    currentPlaylist = {
        tracks: tracks,
        trElts: trElts
    }
}

function getRowInfo (trElt) {
    // get the last link in the cell (if more than one, first is artist)
    var tr = $(trElt);
    var url = tr.find('td.subjectCell a').last().attr('href');
    var artist = url.toString().split('/')[2];
    artist = unescape(artist);
    artist = artist.replace(/\+/g, ' ');
    artist = unescape(artist);

    var track = url.toString().split('/')[4];
    track = unescape(track);
    track = track.replace(/\+/g, ' ');
    track = unescape(track);

    var trackInfo = {
        artist: artist,
        track: track,
        lastfmId: tr.attr('data-track-id')
    };

    if (tr.attr('data-spotify-unresolvable')) {
        trackInfo.unresolvable = true;
    }
    else {
        var spotifyId = tr.attr('data-spotify-id');
        if (spotifyId) {
            trackInfo.spotifyId = spotifyId;
        }
    }

    return trackInfo;
}

$(function() {
    var script = document.createElement( 'script' );
    script.type = 'text/javascript';
    script.src = extension.getURL('lastfm-internal.js');
    document.body.appendChild(script);

    document.addEventListener('lastfmSession', function (event) {
        var sessionJSON = $(document.body).attr('data-session');
        spotfm.setLastfmSession(JSON.parse(sessionJSON));
    });

    trackDetailIframe = $('<iframe class="track-detail-frame"/>');
    trackDetailIframe.hide();
    $(document.body).append(trackDetailIframe);
    var frameBody = trackDetailIframe.get(0).contentDocument.body;
    $(frameBody).append('<link rel="stylesheet" href="' + extension.getURL('detail-frame.css') + '"/><div class="track-detail"></div>');

});
