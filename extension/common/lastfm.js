document.window.addEvent('click', function(theEvent) {
    var target = theEvent.target;
    var parents = target.getParents('.candyStriped, .tracklist');
    if (parents.length !== 0) {
        // get the name of the track
        var trElt = target.getParent('tr');
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
        console.log(text);

        spotfm.resolveAndPlay({artist: artist, track: text}, {
            onResolutionFailure: function () {
                console.log("resolution failed");
                // TODO without adding a class, the data attribute doesn't seem
                // to be noticed until the mouse moves out of the row
                trElt.addClass('thunk');
                trElt.setProperty('data-spotify-unresolvable', 'true');
            }
        });
    }
});
