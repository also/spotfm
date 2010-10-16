var className = ".preview_icon";
var body = $$('table.candyStriped tbody')[0];

body.getChildren().each(function (item){
    item.addEvent('click', function(){
        // get the name of the track
        var subjectCell = this.getChildren('td.subjectCell');
        var text = subjectCell.get('text')[0];

        var url = subjectCell.getChildren('a')[0].get('href');
        var artist = url.toString().split('/')[2];
        artist = unescape(artist);
        artist = artist.replace(/\+/g, ' ');
        artist = unescape(artist);

        var query = [escape(artist), escape(text)].join('+');
        var requestURL = 'http://ws.spotify.com/search/1/track.json?q=' + query;
        var jsonRequest = new Request.JSON({url: requestURL, method:'get', headers: {}, onSuccess: function(json_object, json_string){
            console.log(json_string);
            }
        });
        jsonRequest.send();
        }
    );
    }
);
