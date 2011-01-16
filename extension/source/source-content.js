var omnifm = {
    call: function (action, options) {
        extension.send({action: action, options: options});
    },

    onPlay: function () {
        this.call('onPlay');
    },

    onStop: function () {
        this.call('onStop');
    },

    onNext: function () {
        this.call('onNext');
    },

    onPositionChange: function (position) {
        this.call('onPositionChange', position);
    }
};

omnifm.call('sourceLoaded');
