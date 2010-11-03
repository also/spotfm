document.window.addEvent('click', function(theEvent) {
    var target = theEvent.target;
    var parents = target.getParents('.candyStriped');
    if (parents.length !== 0) {
        // get the name of the track
        var trElt = target.getParent('tr');
        var urlElement = trElt.getChildren('td.subjectCell a')[0];
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

        spotfm.resolveAndPlay({artist: artist, track: text});
    }
});
