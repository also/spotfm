document.window.addEvent('click', function(theEvent) {
    console.log('click!');
    var target = theEvent.target;
    var parents = target.getParents('.candyStriped');
    if (parents.length !== 0){

        // get the name of the track
        if (! target.className === "subjectCell"){
            var subjectCell = target.getParents('td.subjectCell')[0];
        }
        else {
            var subjectCell = target;
        }
        var text = subjectCell.get('text');

        var urlElement = subjectCell.getElements('a');
        var url = urlElement[0].get('href');
        var artist = url.toString().split('/')[2];
        artist = unescape(artist);
        artist = artist.replace(/\+/g, ' ');
        artist = unescape(artist);

        var query = [escape(artist), escape(text)].join('+');
        var requestURL = 'http://ws.spotify.com/search/1/track.json?q=' + query;
        console.log(requestURL);
        var jsonRequest = new Request.JSON({url: requestURL, method:'get', headers: {}, onSuccess: function(json_object, json_string){
            var tracks = json_object.tracks;
            if (tracks.length !== 0){
                var track = tracks[0];
                var id = track.href;
                console.log([track.artists[0].name, track.album.name, track.name].join(' '));
                console.log(id);
                var audioRequest = new Request({url: "http://localhost:9999/play/" + id, method:'get'});
                audioRequest.send();
            }
            else {
                console.log("couldn't resolve");
            }
        }
        });
        jsonRequest.send();
    }
});


