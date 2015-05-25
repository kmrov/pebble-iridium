var xhrRequest = function (url, type, callback) {
    var xhr = new XMLHttpRequest();
    xhr.onload = function () {
        callback(this.responseText);
    };
    xhr.open(type, url);
    xhr.send();
};

var flares = [];

function sendFlare(num) {
    if (flares[num] == undefined)
    {
        return;
    }
    var dict = {
        "KEY_NUM": num,
        "KEY_IRIDIUM_TIME": (flares[num].date.getTime()/1000) - (flares[num].date.getTimezoneOffset()*60),
        "KEY_IRIDIUM_BRIGHTNESS": flares[num].brightness,
        "KEY_IRIDIUM_AZIMUTH": flares[num].azimuth
    };
    console.log("Sending flare " + num + " " + flares[num].date);
    Pebble.sendAppMessage(dict, 
        function (e) {
            console.log("Flare sent successfully: " + num);
            if (num < 3) {
                sendFlare(num+1);
            }
        },
        function (e) {
            console.log("Flare sending failed.");
        }
    );

}

function parseIridiumFlares(str) {
    var re = /<a href=\"flaredetails.aspx.+?">(.+?)<\/a><\/td><td align="center">(.+?)<\/td><td align="center">.+?<\/td><td align="center">(\d+).+?<\/td>/g;
    var flares = [];
    while ((res = re.exec(str)) != null) {
        var year = new Date().getFullYear();
        var date_string = res[1].split(",")[0] + ", " + year + res[1].split(",")[1] + " GMT";
        flares.push(
        {
            date: new Date(date_string),
            brightness: res[2],
            azimuth: res[3]
        }
        );
    }
    return flares;
}

function locationSuccess(pos) {
    var url = "http://www.heavens-above.com/IridiumFlares.aspx?lat=" + pos.coords.latitude +
              "&lng=" + pos.coords.longitude + "&alt=150&tz=UCT";

    xhrRequest(url, 'GET', 
        function(responseText) {
            flares = parseIridiumFlares(responseText);
            sendFlare(0);
        }
    );
}

function locationError(err) {
    console.log('Error requesting location!');
}

function getIridium() {
    navigator.geolocation.getCurrentPosition(
        locationSuccess,
        locationError,
        {timeout: 15000, maximumAge: 60000}
    );
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
    function(e) {
        console.log('PebbleKit JS ready! ' + Date());
        getIridium();
    }
);

Pebble.addEventListener('appmessage',
    function(e) {
        console.log('AppMessage received!');
        getIridium();
    }                     
);
