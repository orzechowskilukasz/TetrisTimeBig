var xhrRequest = function (url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    callback(this.responseText);
  };
  xhr.open(type, url);
  xhr.send();
};


function isIsNightYet(latitude, longitude) {
  const lat = parseFloat(latitude);
  const lon = parseFloat(longitude);
  const date = new Date();

  if (isNaN(lat) || isNaN(lon)) {
    throw new Error("Latitude and longitude must be valid numbers.");
  }

  // 1. Get Day of the Year (N) from 1 to 365
  const startOfYear = new Date(Date.UTC(date.getUTCFullYear(), 0, 1));
  const diffInMs = date.getTime() - startOfYear.getTime();
  const N = Math.floor(diffInMs / (1000 * 60 * 60 * 24)) + 1;

  // 2. Fractional Year (gamma) in radians
  const hour = date.getUTCHours() + date.getUTCMinutes() / 60 + date.getUTCSeconds() / 3600;
  const gamma = (2 * Math.PI / 365) * (N - 1 + (hour - 12) / 24);

  // 3. Equation of Time (eqTime) in minutes (corrects for Earth's elliptical orbit)
  const eqTime = 229.18 * (
    0.000075 +
    0.001868 * Math.cos(gamma) -
    0.032077 * Math.sin(gamma) -
    0.014615 * Math.cos(2 * gamma) -
    0.040849 * Math.sin(2 * gamma)
  );

  // 4. Solar Declination (decl) in radians (the tilt of the Earth relative to the sun)
  const decl = 0.006918 -
    0.399912 * Math.cos(gamma) +
    0.070257 * Math.sin(gamma) -
    0.006758 * Math.cos(2 * gamma) +
    0.000907 * Math.sin(2 * gamma) -
    0.002697 * Math.cos(3 * gamma) +
    0.00148 * Math.sin(3 * gamma);

  // 5. True Solar Time (tst) in minutes
  const timeOffset = eqTime + 4 * lon;
  const tst = (date.getUTCHours() * 60) + date.getUTCMinutes() + (date.getUTCSeconds() / 60) + timeOffset;

  // 6. Hour Angle (ha) in degrees, normalized between -180 and 180
  let ha = (tst / 4) - 180;
  if (ha < -180) ha += 360;
  if (ha > 180) ha -= 360;
  const haRad = ha * Math.PI / 180;

  // 7. Solar Elevation Angle Calculation
  const latRad = lat * Math.PI / 180;
  const sinEl = Math.sin(latRad) * Math.sin(decl) + Math.cos(latRad) * Math.cos(decl) * Math.cos(haRad);
  const elRad = Math.asin(sinEl);
  const elDeg = elRad * 180 / Math.PI;

  // Sunrise/sunset officially occurs when the center of the sun is at -0.833 degrees
  // due to atmospheric refraction and the physical size of the solar disc.
  return elDeg < -0.833 ? 1 : 0;
}

function locationSuccess(pos) {
  var url = 'https://api.open-meteo.com/v1/forecast?' +
      'latitude=' + pos.coords.latitude +
      '&longitude=' + pos.coords.longitude +
      '&current=temperature_2m,weather_code' +
      '&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset' +
      '&timezone=auto';

  xhrRequest(url, 'GET',
    function(responseText) {
      var json = JSON.parse(responseText);

      var temperature = Math.round(json.current.temperature_2m);
      var conditions = json.current.weather_code;
      var isNight = isIsNightYet(pos.coords.latitude, pos.coords.longitude);
      var maxTemperature = Math.round(json.daily.temperature_2m_max[0]);
      var minTemperature = Math.round(json.daily.temperature_2m_min[0]);
      
      // Convert ISO 8601 strings (e.g., "2026-07-19T05:34") to Unix Timestamps (Seconds)
      var sunrise = Math.floor(Date.parse(json.daily.sunrise[0]) / 1000);
      var sunset = Math.floor(Date.parse(json.daily.sunset[0]) / 1000);
      
      var dictionary = {
        'TEMPERATURE': temperature,
        'CONDITIONS': conditions,
        'ISNIGHT': isNight,
        'TEMP_MAX': maxTemperature,
        'TEMP_MIN': minTemperature,
        'SUNRISE': sunrise,
        'SUNSET': sunset
      };

      Pebble.sendAppMessage(dictionary,
        function(e) { console.log('Weather info sent! url: ', url); },
        function(e) { console.log('Error sending weather info!'); }
      );
    }
  );
}

function locationError(err) {
  console.log('Error requesting location!');
}

function getWeather() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    { timeout: 15000, maximumAge: 60000 }
  );
}

Pebble.addEventListener('ready',
  function(e) {
    console.log('PebbleKit JS ready!');
    getWeather();
  }
);

Pebble.addEventListener('appmessage',
  function(e) {
    console.log('AppMessage received!');
    if (e.payload['REQUEST_WEATHER']) {
      getWeather();
    }
  }
);