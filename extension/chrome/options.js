extension.addSettingListener(resetOptions);

function resetOptions() {
    $('#scrobble').attr('checked', extension.getSetting('scrobble'));
};

function save() {
    extension.setSetting('scrobble', $('#scrobble').attr('checked'));
}

$(function () {
    resetOptions();
    $('#ok').bind('click', function (e) {
        save();
        window.close();
    });
});
