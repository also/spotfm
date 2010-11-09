var currentTrElt;

spotfm.next = function() {
    if (currentTrElt) {
        var nextTrElt = currentTrElt.getNext('tr');
        if (nextTrElt) {
            playTrElt(nextTrElt);
        }
    }
};

spotfm.previous = function() {
    if (currentTrElt) {
        var previousTrElt = currentTrElt.getPrevious('tr');
        if (previousTrElt) {
            playTrElt(previousTrElt);
        }
    }
};

spotfm.tryToPlay = function () {
    if (currentTrElt) {
        playTrElt(currentTrElt);
    }
};

spotfm.connect();

document.window.addEvent('click', function(theEvent) {
    var target = theEvent.target;
    var parents = target.getParents('.candyStriped, .tracklist');
    if (parents.length !== 0) {
        var trElt = target.getParent('tr');
        playTrElt(trElt);
    }
});

function playTrElt(trElt) {
    if (currentTrElt) {
        currentTrElt.removeClass('spotfmPlaying');
    }

    currentTrElt = trElt;

    if (trElt.getProperty('data-spotify-unresolvable')) {
        currentTrElt = trElt;
        spotfm.next();
        return;
    }

    trElt.addClass('spotfmPlaying');

    var trackId = trElt.getProperty('data-spotify-id');
    if (trackId) {
        spotfm.play(trackId);
    }
    else {
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

        spotfm.resolveAndPlay({artist: artist, track: text}, {
            onResolution: function (id) {
                trElt.setProperty('data-spotify-id', id);
            },
            onResolutionFailure: function () {
                console.log("resolution failed");
                // TODO without adding a class, the data attribute doesn't seem
                // to be noticed until the mouse moves out of the row
                trElt.addClass('thunk');
                trElt.setProperty('data-spotify-unresolvable', 'true');
                spotfm.next();
            }
        });
    }
}

var checkForSessionStarted;
var lastfmSession;
window.addEvent('domready', function() {
    var scriptElt = new Element('script', {src: safari.extension.baseURI + 'lastfm-internal.js'});
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
        lastfmSession = JSON.parse(sessionJSON);
    }
}
