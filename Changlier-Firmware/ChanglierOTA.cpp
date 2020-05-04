#include "ChanglierOTA.h"


String ssid;
String password;
WebServer server(80);

boolean wifi_enabled;

const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<link rel='stylesheet' href='https://ajax.googleapis.com/ajax/libs/jqueryui/1.12.1/themes/smoothness/jquery-ui.css'>"
"<script src='https://ajax.googleapis.com/ajax/libs/jqueryui/1.12.1/jquery-ui.min.js'></script>"
"<div id='all' style='font-family Arial'>"
"<h3>Upload new firmware</h3>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
"   <input type='file' name='update'>"
"        <input type='submit' value='Update'>"
"    </form>"
" <div id='prg'></div>"
" <script>"
"   $( function() {"
"   } );"
"  $('form').submit(function(e){"
"  e.preventDefault();"
"  var form = $('#upload_form')[0];"
"  var data = new FormData(form);"
"   $.ajax({"
"  url: '/update',"
"  type: 'POST',"
"  data: data,"
"  contentType: false,"
"  processData:false,"
"  xhr: function() {"
"  var xhr = new window.XMLHttpRequest();"
"  xhr.upload.addEventListener('progress', function(evt) {"
"  if (evt.lengthComputable) {"
"  var per = evt.loaded / evt.total;"
"  $('#prg').html('Progress: ' + Math.round(per*100) + '%');"
"  $( '#progressbar' ).progressbar({value: Math.round(per*100) });"
"  }"
"  }, false);"
"  return xhr;"
"  },"
"  success:function(d, s) {"
"   $('#all').html('Done.') "
" },"
" error: function (a, b, c) {"
" }"
" });"
" });"
" </script>"
" <div id='progressbar'></div>"
"<p>Attention: needs internet connection to load jQuery</p>"
 "</div>";
 
void enable_wifi() {
	Serial.println("Enabling Wifi");
	Serial.println(ssid);
	Serial.println(password);

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid.c_str(), password.c_str());
	long start_time = millis();
	 while (WiFi.status() != WL_CONNECTED) { 
		delay(500); 
		if ((millis()-start_time) > WIFI_TIMEOUT) break;
	}

	if (WiFi.status() == WL_CONNECTED) {
		
			String host = hostname;
			host.toLowerCase();
			host.replace(" ", "_");
		 if (!MDNS.begin(host.c_str())) { 
			Serial.println("Error setting up MDNS responder!");
			while (1) {
			  delay(1000);
			}
		  }
		Serial.print("Hostname: ");
		Serial.println(host);
		Serial.println("Wifi OK");

		wifi_enabled = true;

		delay(100);
		btStop();
		delay(200);
		
		fill_solid(pixels, NUM_PIXELS, CRGB::Blue);
		FastLED.show();

		  server.on("/restart", HTTP_GET, []() {
			server.sendHeader("Connection", "close");
			server.send(200, "text/plain", "Rebooting");
			preferences.begin("changlier", false);
			preferences.putInt("wifi",0);
			preferences.end();

			delay(200);
			ESP.restart();
		  });
		
		  server.on("/", HTTP_GET, []() {
			server.sendHeader("Connection", "close");
			server.send(200, "text/html", serverIndex);
		  });
		  server.on("/serverIndex", HTTP_GET, []() {
			server.sendHeader("Connection", "close");
			server.send(200, "text/html", serverIndex);
		  });
		  /*handling uploading firmware file */
		  server.on("/update", HTTP_POST, []() {
			server.sendHeader("Connection", "close");
			server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
			preferences.begin("changlier", false);
			preferences.putInt("wifi",0);
			preferences.end();

			ESP.restart();
		  }, []() {
			HTTPUpload& upload = server.upload();
			if (upload.status == UPLOAD_FILE_START) {
			  Serial.printf("Update: %s\n", upload.filename.c_str());
			  if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
				Update.printError(Serial);
			  }
			} else if (upload.status == UPLOAD_FILE_WRITE) {
			  /* flashing firmware to ESP*/
			  if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
				Update.printError(Serial);
			  }
			} else if (upload.status == UPLOAD_FILE_END) {
			  if (Update.end(true)) { //true to set the size to the current progress
				Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
			  } else {
				Update.printError(Serial);
			  }
			}
		  });
		  server.begin();
  
	} else {
			Serial.println("Error enabling Wifi");
	}
}
