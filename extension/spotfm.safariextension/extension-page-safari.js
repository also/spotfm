extension.send = function (data) {
    safari.self.tab.dispatchMessage('data', data);
};

safari.self.addEventListener('message', function (event) {
    if (extension.onMessage) {
        extension.onMessage(event.message);
    }
}, false);

extension.getURL = function (name) {
    return safari.extension.baseURI + name;
};

extension.setContextMenuEventUserInfo = function (e, userInfo) {
    safari.self.tab.setContextMenuEventUserInfo(e, userInfo);
};