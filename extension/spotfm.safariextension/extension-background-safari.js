extension.send = function (sender, data) {
    sender.dispatchMessage('data', data);
};

safari.application.addEventListener('message', function (event) {
    if (extension.onMessage) {
        extension.onMessage(event.target.page, event.message);
    }
}, false);
