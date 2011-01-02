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

extension.getSetting = function (key) {
    var rawValue = localStorage[key];
    if (rawValue) {
        return JSON.parse(rawValue);
    }
};

extension.setSetting = function (key, value) {
    localStorage[key] = JSON.stringify(value);
};

extension.addSettingListener = function (listener) {
    window.addEventListener('storage', function (e) {
        var parsedEvent = {
            key: e.key,
            newValue: null,
            oldValue: null
        };
        if (e.oldValue) {
            parsedEvent.oldValue = JSON.parse(e.oldValue);
        }
        if (e.newValue) {
            parsedEvent.newValue = JSON.parse(e.newValue);
        }
        listener(e);
    }, false);
};
