var play = true;
var ws = new WebSocket('ws://localhost:9999/monitor');

ws.onmessage = function(message){
    var data = message.data;
    console.log(message.data);
    //play = data;
    //chrome.browserAction.setIcon({path:getImage(play)});
}

ws.onopen = function(open){
    console.log('the web socket was opened');
}

ws.onclose = function(close){
    console.log('the web socket was closed');
}

function getImage(state){
    if (state){
        return "play.jpeg";
    }
    else {
        return "pause.jpeg";
    }
}
chrome.browserAction.onClicked.addListener(function() {
//    play = !play; 
//    chrome.browserAction.setIcon({path:getImage(play)});
});
