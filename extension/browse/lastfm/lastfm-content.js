var trackRows = $('.candyStriped td, .tracklist td');

trackRows.live('click', function (event) {
    var target = event.target;
    // if ($(target).closest('td.playbuttonCell').length > 0) {
    //     showDetail(event);
    //     return;
    // }
    var trElt = $(target).closest('tr').get(0);
    playTrElt(trElt);
});

if (extension.setContextMenuEventUserInfo) {
    trackRows.live('contextmenu', function (event) {
        var userInfo = {trackInfo: getRowInfo($(event.target).closest('tr').get(0))};
        extension.setContextMenuEventUserInfo(event, userInfo);
    });

    $('a').live('contextmenu', function (event) {
        var url = $(event.target).closest('a').attr('href');
        if (url.indexOf('/music/') === 0) {
            var userInfo = {objectInfo: parseUrl(url)};
            extension.setContextMenuEventUserInfo(event, userInfo);
        }
    });
}

function deLastfmify(segment) {
    return unescape(unescape(segment).replace(/\+/g, ' '));
}

function parseUrl(url) {
    var parts = url.split('/').map(deLastfmify);
    var result = {
        type: 'artist',
        artist: parts[2],
    };

    if (parts.length >= 4 && parts[3][0] != '_' && parts[3][0] != ' ') {
        result.type = 'album';
        result.album = parts[3];
    }
    if (parts.length >= 5 && parts[4][0] != ' ') {
        result.type = 'track';
        result.track = parts[4];
    }

    return result;
}

function getRowInfo(trElt) {
    // get the last link in the cell (if more than one, first is artist)
    var tr = $(trElt);
    var url = tr.find('td.subjectCell a').last().attr('href');

    var trackInfo = parseUrl(url);

    trackInfo.lastfmId = tr.attr('data-track-id');
    trackInfo.unresolvable = !!tr.attr('data-spotify-unresolvable');
    trackInfo.spotifyId = tr.attr('data-spotify-id');

    return trackInfo;
}

$(function() {
    var script = document.createElement( 'script' );
    script.type = 'text/javascript';
    script.src = extension.getURL('lastfm-internal.js');
    document.body.appendChild(script);

    document.addEventListener('lastfmSession', function (event) {
        var sessionJSON = $(document.body).attr('data-session');
        omnifm.setLastfmSession(JSON.parse(sessionJSON));
    });
});
