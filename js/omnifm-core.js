var omnifm = {
    player: {
        position: 0,
        onPlay: function () {
            omnifm.setPosition(omnifm.player.position);
            omnifm.positionUpdateInterval = window.setInterval(omnifm.updatePosition, 500);
        },

        onStop: function () {
            window.clearInterval(omnifm.positionUpdateInterval);
        }
    },

    resolveAndPlay: function (trackInfo, options) {
        options = options || {};
        var onResolution = options.onResolution;
        options.onResolution = function(trackId) {
            if (onResolution) {
                onResolution(trackId);
            }
            omnifm.play(trackId);
        }
        omnifm.resolve(trackInfo, options);
    },

    setPosition: function (position) {
        omnifm.player.position = position;
        omnifm.timeOffset = new Date().getTime() - position;
        if (omnifm.onPositionChange) {
            omnifm.onPositionChange(position);
        }
    },

    updatePosition: function () {
        var now = new Date().getTime();
        var played = now - omnifm.timeOffset;
        if (played >= omnifm.player.duration) {
            window.clearInterval(omnifm.positionUpdateInterval);
            played = omnifm.player.duration;
        }
        if (omnifm.onPositionChange) {
            omnifm.onPositionChange(omnifm.player.position);
        }
    },

    onSourceConnect: function () {
        omnifm.sourceConnected = true;
        omnifm.sourceConnecting = false;
        console.log('onSourceConnect');
        if (omnifm._sourceConnectCallback) {
            omnifm._sourceConnectCallback();
            omnifm._sourceConnectCallback = null;
        }
    },

    onSourceConnectionError: function () {
        console.log('onSourceConnectionError');
        omnifm.sourceConnected = false;
        omnifm.sourceConnecting = false;
    },

    onSourceDisconnect: function () {
        console.log('onSourceDisconnect');
        if (!omnifm.sourceConnected) {
            console.log('failed to connect');
        }
        omnifm.sourceConnected = false;
        omnifm.sourceConnecting = false;
    },

    connect: function () {
        console.log('trying to connect');
        omnifm.sourceConnected = false;
        omnifm.sourceConnecting = true;
        omnifm.source.connect();
    }
};

['play', 'pause', 'resume', 'resolve'].forEach(function (name) {
    omnifm[name] = function () {
        var args = arguments;
        if (omnifm.sourceConnected) {
            this.source[name].apply(this.source, args);
        }
        else {
            omnifm._sourceConnectCallback = function () {
                this.source[name].apply(this.source, args);
            }
            omnifm.connect();
        }
    };
});
