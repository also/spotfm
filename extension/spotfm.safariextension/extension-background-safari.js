extension.send = function (sender, data) {
    console.log('send', data);
    sender.dispatchMessage('data', data);
};

safari.application.addEventListener('message', function (event) {
    if (extension.onMessage) {
        extension.onMessage(event.target.page, event.message);
    }
}, false);
