document.window.addEvent('click', function(theEvent) {
    var target = theEvent.target;
    var parents = target.getParents('.candyStriped');
    if (parents.length !== 0){

        // get the name of the track
        var subjectCell = target.getParents('td.subjectCell')[0];
        var text = subjectCell.get('text');

        var urlElement = subjectCell.getElements('a');
        var url = urlElement[0].get('href');
        var artist = url.toString().split('/')[2];
        artist = unescape(artist);
        artist = artist.replace(/\+/g, ' ');
        artist = unescape(artist);

        var query = [escape(artist), escape(text)].join('+');
        var requestURL = 'http://ws.spotify.com/search/1/track.json?q=' + query;
        var jsonRequest = new Request.JSON({url: requestURL, method:'get', headers: {}, onSuccess: function(json_object, json_string){
            var track = json_object.tracks[0];
            //console.log([track.artists[0].name, track.album.name, track.name].join(' '));
            var id = track.href;
            console.log(id);
            var audioRequest = new Request({url: "http://localhost:9999/play/" + id, method:'get'});
            audioRequest.send();
            }
        });
        jsonRequest.send();
    }
});


