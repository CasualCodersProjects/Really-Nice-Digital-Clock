const char HTML[] PROGMEM = R"=====(
    <!doctype html>
    <html lang="en">
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>Nice Clock</title>
        <link rel="stylesheet" href="/bootstrap.css">
        <script src="/bootstrap.js"></script>
    </head>
    
    <body>
        <h1 class="text-center p-1">A Really Nice Digital Clock</h1>
        <br>

        <div class="d-grid gap-2 col-8 mx-auto text-center">
        <h2>Set your timezone:</h2>
        <select class="form-control" id="timezone">Select timezone</select>
        <button type="button" class="btn btn-primary">Update</button>
        </div>
        <br>

        <div class="d-grid gap-2 col-8 mx-auto text-center">
        <h2>WiFi:</h2>
        <input type="ssid" class="form-control" id="ssid" placeholder="SSID">
        <input type="psk" class="form-control" id="psk" placeholder="Password">
        <button type="button" class="btn btn-primary" id="updateWiFi">Update</button>
        </div>
        <br>

        <div class="d-grid gap-2 col-8 mx-auto text-center">
        <h2>Brightness:</h2>
        <label for="minBrightness" class="form-label">Minimum Brightness</label>
        <input type="range" class="form-range" min="0" max="255" id="minBrightness" value="20">
        <label for="maxBrightness" class="form-label">Maximum Brightness</label>
        <input type="range" class="form-range" min="0" max="255" id="maxBrightness" value="255">
        </div>
        <br>
        <div class="alert alert-primary d-grid gap-2 col-8 mx-auto text-center dissappear"  role="alert" id="TZUpdated">
        Timezone Updated
        </div>
        
        <br>



        <script src="./timezones/moment.min.js"></script>
        <script src="./timezones/moment-timezone.min.js"></script>
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

        document.getElementById("updateWiFi").addEventListener("click", function(event){
            event.preventDefault();
            var xhr = new XMLHttpRequest();
            xhr.open('POST', '/setwiFi', true);
            xhr.setRequestHeader('Content-Type', 'text/plain');
            xhr.send(document.getElementById("ssid").value);
            xhr,send(document.getElementById("psk").value);
        });



        </script>
    </body>
    </html>
)=====";