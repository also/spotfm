var spotify = {
    connected: false,
    connecting: false,

    play: function (trackId) {
        $.get('http://localhost:9999/play/' + trackId);
    },

    pause: function () {
        $.get('http://localhost:9999/stop');
    },

    resume: function () {
        $.get('http://localhost:9999/resume');
    },

    connect: function () {
        this.connected = false;
        this.connecting = true;

        if (this.ws) {
            return;
        }
        console.log('trying to connect');
        var ws = new WebSocket('ws://localhost:9999/monitor');
        this.ws = ws;
        ws.onopen = function () {
            console.log('open');
            this.connecting = false;
            this.connected = true;
        };

        ws.onerror = function () {
            this.ws = null;
            this.connected = false;
            this.connecting = false;
            console.log('spotfm connection error');
            window.setTimeout(this.connect, 1000);
        };

        ws.onclose = function () {
            this.ws = null;
            this.connected = false;
            this.connecting = false;
            console.log('spotfm connection closed');
            window.setTimeout(this.connect, 1000);
        };

        ws.onmessage = function (a) {
            if (a.data.length > 0) {
                var j = JSON.parse(a.data);
                if (j.event == 'track_info') {
                    omnifm.player.trackInfo = j;
                    omnifm.player.duration = j.track_duration;
                    if (omnifm.onTrackInfo) {
                        omnifm.onTrackInfo(j);
                    }
                }
                else if (j.event == 'start_of_track') {
                    if (omnifm.onTrackStart) {
                        omnifm.onTrackStart();
                    }
                }
                else if (j.event == 'playing') {
                    omnifm.player.onPlay();
                    if (omnifm.onPlay) {
                        omnifm.onPlay();
                    }
                }
                else if (j.event == 'stopped') {
                    omnifm.player.onStop();
                    if (omnifm.onStop) {
                        omnifm.onStop();
                    }
                }
                else if (j.event == 'position') {
                    omnifm.setPosition(j.position);
                }
                else if (j.event == 'next' || j.event == 'end_of_track' || j.event == 'playback_failed') {
                    if (omnifm.next) {
                        omnifm.next();
                    }
                }
                else if (j.event == 'previous') {
                    if (omnifm.previous()) {
                        omnifm.previous();
                    }
                }
                else if (j.event == 'play') {
                    if (omnifm.tryToPlay) {
                        omnifm.tryToPlay();
                    }
                }
            }
        };
    }
};
