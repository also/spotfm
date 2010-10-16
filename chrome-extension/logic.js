var className = ".preview_icon";
var body = $$('table.candyStriped tbody')[0]

body.getChildren().each(function (item){
    item.addEvent('click', function(){
        // get the name of the track
        var subjectCell = this.getChildren('td.subjectCell');
        var text = subjectCell.get('text');

        var url = subjectCell.getChildren('a')[0].get('href');
        var artist = url.toString().split('/')[2]
        artist = unescape(artist);
        artist = artist.replace(/\+/g, ' ');
        artist = unescape(artist);
        alert(artist);
        }
    );
    }
);
