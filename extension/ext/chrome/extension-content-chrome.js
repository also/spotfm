extension.port = chrome.extension.connect();

extension.send = function (data) {
    extension.port.postMessage(data);
};

extension.port.onMessage.addListener(function (message) {
    if (extension.onMessage) {
        extension.onMessage(message);
    }
});

extension.getURL = chrome.extension.getURL;
