_.extend(spotify, {
    openTrackInApp: function (trackInfo) {
        omnifm.resolve(trackInfo, {
            onResolution: function (id) {
                extension.openExternalUri(id);
            },
            onResolutionFailure: function () {
                extension.openExternalUri('spotify:search:' + spotify.generateTrackQuery(trackInfo));
            }
        });
    },

    searchTrackInApp: function (trackInfo) {
        extension.openExternalUri('spotify:search:' + spotify.generateTrackQuery(trackInfo));
    },

    searchObjectInApp: function (info) {
        extension.openExternalUri('spotify:search:' + spotify.generateQuery(info));
    }
});
