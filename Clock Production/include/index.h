const char HTML[] PROGMEM = R"=====(
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Nice Clock</title>
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0-alpha1/dist/css/bootstrap.min.css" rel="stylesheet" integrity="sha384-GLhlTQ8iRABdZLl6O3oVMWSktQOp6b7In1Zl3/Jr59b6EGGoI1aFkw7cmDA6j6gD" crossorigin="anonymous">
  </head>
  
  <body>
    <h1 class="text-center p-1">A Really Nice Digital Clock</h1>
    <br>

    <div class="d-grid gap-2 col-8 mx-auto text-center">
      <h2>Set your timezone:</h2>
      <select class="form-control" id="timezone">Select timezone</select>
      <button type="button" class="btn btn-primary" id="tzUpdate">Update</button>
    </div>
    <br>

    <div class="d-grid gap-2 col-8 mx-auto text-center">
      <h2>WiFi:</h2>
      <input type="ssid" class="form-control" id="ssid" placeholder="SSID">
      <input type="psk" class="form-control" id="psk" placeholder="Password">
      <button type="button" class="btn btn-primary">Update</button>
    </div>
    <br>

    <div class="d-grid gap-2 col-8 mx-auto text-center">
      <h2>Brightness:</h2>
      <label for="minBrightness" class="form-label">Minimum Brightness</label>
      <input type="range" class="form-range" min="0" max="255" id="minBrightness">
      <label for="maxBrightness" class="form-label">Maximum Brightness</label>
      <input type="range" class="form-range" min="0" max="255" id="maxBrightness">
    </div>
    <br>



    <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0-alpha1/dist/js/bootstrap.bundle.min.js" integrity="sha384-w76AqPfDkMBDXo30jS1Sgez6pr3x5MlQ1ZAGC+nuZB+EYdgRZgiwxhTBTkF7CXvN" crossorigin="anonymous"></script>

    <script src="https://cdnjs.cloudflare.com/ajax/libs/moment.js/2.29.4/moment.min.js"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/moment-timezone/0.5.31/moment-timezone-with-data-10-year-range.min.js"></script>
    <script>
      // Generate a list of timezones using Moment.js
      var timezones = moment.tz.names();

      // Populate the timezone dropdown
      for (var i in timezones) {
        var option = document.createElement("option");
        option.value = timezones[i];
        var offset = moment.tz(timezones[i]).format('Z');
        option.text = '('+offset+') - ' + timezones[i];
        document.getElementById("timezone").add(option);
      }

      document.getElementById("timezone").addEventListener("change", function(event){
        event.preventDefault();
        var xhr = new XMLHttpRequest();
        xhr.open('POST', '/setTZ', true);
        xhr.setRequestHeader('Content-Type', 'text/plain');
        xhr.send(event.target.value);
      });



    </script>
  </body>
</html>
)=====";