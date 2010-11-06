var currentTrElt;

function connect() {
    console.log('trying to connect');
    ws = new WebSocket('ws://localhost:9999/monitor');

    ws.onmessage = function (a) {
        var j = JSON.parse(a.data);
        if (j.event == 'advance' || j.event == 'end_of_track' || j.event == 'playback_failed') {
            console.log(j.event);
            if (spotfm.advance) {
                spotfm.advance();
            }
        }
        else if (j.event == 'play') {
            if (currentTrElt) {
                playTrElt(currentTrElt);
            }
        }
    };

    ws.onopen = function () {
      console.log('connected');
    };

    ws.onerror = function () {
      console.log('spotfm error');
      window.setTimeout(connect, 1000);
    };

    ws.onclose = function () {
       console.log('spotfm closed');
       window.setTimeout(connect, 1000);
    };
}

connect();

spotfm.advance = function() {
    if (currentTrElt) {
        var nextTrElt = currentTrElt.getNext('tr');
        if (nextTrElt) {
            playTrElt(nextTrElt);
        }
    }
};

document.window.addEvent('click', function(theEvent) {
    var target = theEvent.target;
    var parents = target.getParents('.candyStriped, .tracklist');
    if (parents.length !== 0) {
        // get the name of the track
        var trElt = target.getParent('tr');
        playTrElt(trElt);
    }
});

function playTrElt(trElt) {
    if (currentTrElt) {
        currentTrElt.removeClass('spotfmPlaying');
    }
    trElt.addClass('spotfmPlaying');
    // get the last link in the cell (if more than one, first is artist)
    var urlElement = trElt.getChildren('td.subjectCell a').pop();
    var url = urlElement.get('href');
    var artist = url.toString().split('/')[2];
    artist = unescape(artist);
    artist = artist.replace(/\+/g, ' ');
    artist = unescape(artist);

    var text = url.toString().split('/')[4];
    text = unescape(text);
    text = text.replace(/\+/g, ' ');
    text = unescape(text);

    currentTrElt = trElt;

    spotfm.resolveAndPlay({artist: artist, track: text}, {
        onResolutionFailure: function () {
            console.log("resolution failed");
            // TODO without adding a class, the data attribute doesn't seem
            // to be noticed until the mouse moves out of the row
            trElt.addClass('thunk');
            trElt.setProperty('data-spotify-unresolvable', 'true');
            spotfm.advance();
        }
    });
}
