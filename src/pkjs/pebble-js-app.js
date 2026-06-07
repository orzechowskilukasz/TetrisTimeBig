Pebble.addEventListener("ready",
    function(e) {
        console.log("js app inited");
        if (window.localStorage.getItem("options") === null) {
            Pebble.sendAppMessage({},
                                  function(e) {},
                                  function(e) {
                                      console.log('Error sending message to watch: ' + JSON.stringify(e));
                                  });
        }
    }
);

// Pebble.addEventListener("appmessage",
//    function(e) {
//        console.log("Got settings: " + JSON.stringify(e.payload));
//        window.localStorage.options = JSON.stringify(e.payload);
//    }
// );


Pebble.addEventListener("appmessage",
    function(e) {
        console.log("Got message from watch: " + JSON.stringify(e.payload));

        if (e.payload['MESSAGE_KEY_REQUEST_WEATHER'] !== undefined || e.payload['REQUEST_WEATHER'] !== undefined) {
            
            console.log("Received weather request, not my job, ignoring");
            // fetchWeather(); 
        } else {
            if (e.payload['0'] !== undefined || e.payload['VERSION'] !== undefined) {
                console.log("Looks like my settings. Saving to localStorage.");
                window.localStorage.options = JSON.stringify(e.payload);
            } else {
                console.log("Uknown message type. Go away.");
            }
        }
    }
);

Pebble.addEventListener("showConfiguration",
    function(e) {
      var url = 'http://thorn.piekielko.pl/tetristimebig/configuration.html';
        var options = window.localStorage.getItem("options");
        if (options !== null) {
            options = escape(options);
            console.log('Showing config with options=' + options);
            Pebble.openURL(url + '?options=' + options);
        } else {
            console.log('Showing config with no options');
            Pebble.openURL(url);
        }
    }
);

Pebble.addEventListener("webviewclosed",
    function(e) {
        console.log('Got response: ' + e.response);
        var config = JSON.parse(decodeURIComponent(e.response));
        
        window.localStorage.options = JSON.stringify(config);
        Pebble.sendAppMessage(config,
                              function(e) {},
                              function(e) {
                                  console.log('Error sending message to watch: ' + JSON.stringify(e));
                              });
    }
);

 
