var play = true;
//var ws = new WebSocket('ws://localhost:9998/');

/*
ws.onmessage = function(message){
    var data = message.data;
    play = data;
    chrome.browserAction.setIcon({path:getImage(play)});
}
*/

function getImage(state){
    if (state){
        return "play.jpeg";
    }
    else {
        return "pause.jpeg";
    }
}
chrome.browserAction.onClicked.addListener(function() {
    play = !play; 
    chrome.browserAction.setIcon({path:getImage(play)});
});
