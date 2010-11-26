document.body.setAttribute('data-session', JSON.stringify(LFM.Session));

var event = document.createEvent('Event');
event.initEvent('lastfmSession', true, true);
document.dispatchEvent(event);
