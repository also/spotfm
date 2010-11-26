var currentTrElt;
var currentPlaylist;
var currentPlaylistOffset = null;

document.window.addEvent('click', function (event) {
    var target = event.target;
    var parents = target.getParents('.candyStriped, .tracklist');
    if (parents.length !== 0) {
        var trElt = target.getParent('tr');
        playTrElt(trElt);
    }
});

document.window.addEvent('unload', function (event) {
    console.log('unload');
    spotfm.pageUnloaded();
});

extension.onMessage = function(message) {
    console.log('onMessage', message);

    var action = message.action;
    var options = message.options;

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

function onResolution (offset, id) {
    var trElt = currentPlaylist.trElts[offset];
    trElt.setProperty('data-spotify-id', id);
}

function onResolutionFailure (offset) {
    var trElt = currentPlaylist.trElts[offset];
    // TODO without adding a class, the data attribute doesn't seem
    // to be noticed until the mouse moves out of the row
    trElt.addClass('thunk');
    trElt.setProperty('data-spotify-unresolvable', 'true');
}

function onPlay (offset)  {
    if (currentPlaylistOffset != null) {
        currentPlaylist.trElts[currentPlaylistOffset].removeClass('spotfmPlaying');
    }

    var trElt = currentPlaylist.trElts[offset];
    trElt.addClass('spotfmPlaying');
    currentPlaylistOffset = offset;
}

function playTrElt(trElt) {
    var trElts = trElt.getParent().getChildren('tr');

    var tracks = trElts.map(getRowInfo);

    spotfm.playFromPlaylist(tracks, trElts.indexOf(trElt));

    currentPlaylist = {
        tracks: tracks,
        trElts: trElts
    }
}

function getRowInfo (trElt) {
    // get the last link in the cell (if more than one, first is artist)
    var urlElement = trElt.getChildren('td.subjectCell a').pop();
    var url = urlElement.get('href');
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
        lastfmId: trElt.getProperty('data-track-id')
    };

    if (trElt.getProperty('data-spotify-unresolvable')) {
        trackInfo.unresolvable = true;
    }
    else {
        var spotifyId = trElt.getProperty('data-spotify-id');
        if (spotifyId) {
            trackInfo.spotifyId = spotifyId;
        }
    }

    return trackInfo;
}

var checkForSessionStarted;
var lastfmSession;
window.addEvent('domready', function() {
    var scriptElt = new Element('script', {src: extension.getURL('lastfm-internal.js')});
    document.body.appendChild(scriptElt);
    checkForSessionStarted = new Date().getTime();
    window.setTimeout(checkForSession, 100);
});

function checkForSession () {
    var sessionJSON = document.body.getProperty('data-session');
    if (!sessionJSON) {
        if (new Date().getTime() - checkForSessionStarted < 1000) {
            window.setTimeout(checkForSession, 100);
        }
    }
    else {
        spotfm.setLastfmSession(JSON.parse(sessionJSON));
    }
}
