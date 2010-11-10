extension.send = function (sender, data) {
    sender.target.page.dispatchMessage('data', data);
};

safari.application.addEventListener('message', function (event) {
    if (extension.onMessage) {
        extension.onMessage(event, event.message);
    }
}, false);

