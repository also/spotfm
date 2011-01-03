$('.candyStriped td, .tracklist td').live('click', function (event) {
    var target = event.target;
    // if ($(target).closest('td.playbuttonCell').length > 0) {
    //     showDetail(event);
    //     return;
    // }
    var trElt = $(target).closest('tr').get(0);
    playTrElt(trElt);
});

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
});
