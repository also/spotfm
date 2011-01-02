extension.send = function (sender, data) {
    sender.dispatchMessage('data', data);
};

safari.application.addEventListener('message', function (event) {
    if (extension.onMessage) {
        extension.onMessage(event.target.page, event.message);
    }
}, false);

extension.getSetting = function (key) {
    return safari.extension.settings[key];
}

extension.setSetting = function (key, value) {
    safari.extension.settings[key] = value;
}

extension.addSettingListener = function (listener) {
    safari.extension.settings.addEventListener('change', listener, false);
};
