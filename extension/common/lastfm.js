var ws = new WebSocket('ws://localhost:9999/monitor');
ws.onopen = function() {
  console.log('open');
	ws.send('oh hai');
};

/*ws.onerror = function () {
  console.log('error');
};*/

ws.onclose = function() {
   console.log('close');
};

ws.onmessage = function(m) {
   console.log('onmessage', m.data);
	ws.send('ok: ' + m.data);
};
console.log(ws);

document.window.addEvent('click', function(theEvent) {
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

        var urlElement = subjectCell.getElements('a');
        var url = urlElement[0].get('href');
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


