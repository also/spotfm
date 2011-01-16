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
    }
};

['play', 'pause', 'resume', 'resolve', 'connect'].forEach(function (name) {
    omnifm[name] = function () {
        this.source[name].apply(this.source, arguments);
    };
});
