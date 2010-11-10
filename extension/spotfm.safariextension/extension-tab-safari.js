extension.send = function (data) {
    safari.self.tab.dispatchMessage('data', data);
};

safari.self.addEventListener('message', function (event) {
    console.log(event);
}, false);
