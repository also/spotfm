extension.send = function (port, message) {
    port.postMessage(message);
};

chrome.extension.onConnect.addListener(function (port) {
    port.onMessage.addListener(function (message) {
        if (extension.onMessage) {
            extension.onMessage(port, message);
        }
    });
});
