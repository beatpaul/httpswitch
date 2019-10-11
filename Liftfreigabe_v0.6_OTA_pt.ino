#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>

#include <ESPmDNS.h>
#include <Update.h>
#include  "ping.h"


WebServer http_server ( 80 );

WebServer http_OTA_server ( 8080 );

const char* host = "liftfreigabe";                                             // <<< ---- OTA Host  ------------------

const char* ssid     = "YOUR SSID";                                               // <<< ---- WIFI Settings  ------------------
const char* password = "YOUR PW";


/*
 * Login page OTA
 */

const char* loginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>Liftfreigabe OTA (v0.6_pt)</b></font></center>"    // <<< ---- OTA Name + VERSION!!------------------
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='YOUR USERNAME' && form.pwd.value=='YOUR OTA PW')"                   // <<< ---- OTA Password ------------------
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";
 
/*
 * Server Index Page / OTA
 */

const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";


// ---------------------------------------------------------------------------------



IPAddress local_IP(10, 0, 1, 130);                          // <<< ---- IP Settings ------------------
IPAddress gateway(10, 0, 1, 138);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(10, 0, 1, 138); //optional
IPAddress secondaryDNS(8, 8, 8, 8); //optional

const IPAddress Ping_IP(10, 0, 1, 100);
int Ping_check = 0;
int Wifi_check = 0;


int Relais_1_GPIO = 23;                              //<<<<----- Enter GPIO   Relais ---- 
int Relais_2_GPIO = 4;


bool Relais_1_Status = 0;
bool Relais_2_Status = 0;


bool Relais_1_Stateless = 0;
bool Relais_2_Stateless = 0;

uint32_t Stateless_Time1 = 0;
uint32_t Stateless_Time2 = 0;
uint32_t StatelessInt = 1500; 



void html_header(){
  http_OTA_server.sendHeader("Access-Control-Allow-Origin","*");
  http_OTA_server.sendHeader("Access-Control-Allow-Credentials", "true");
  http_OTA_server.sendHeader("Access-Control-Allow-Methods", "GET,HEAD,OPTIONS,POST,PUT");
  http_OTA_server.sendHeader("Access-Control-Allow-Headers", "Access-Control-Allow-Headers, Origin,Accept, X-Requested-With, Content-Type, Access-Control-Request-Method, Access-Control-Request-Headers, Authorization");
}



//-------------------------------------------------------------------------------------------------


void setup() {                  // <<<<<------------- put your setup code here, to run once:


// --------------- Wifi connect --------------------------------------------------

  Serial.begin(115200);
  
  WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(". ");
    Serial.println(Wifi_check);
    Wifi_check++;
    if (Wifi_check > 15) {
                Wifi_check = 0;
                Serial.println("Wifi Restart!");
                 delay(1000);
                
                ESP.restart();
                }



    
  }
  
 Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Host: http://");
  Serial.print(host);
  Serial.println(".local:8080");

  

/*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }


//----------------------------- OTA --------------------------------------------------------------------

Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  http_OTA_server.on("/", HTTP_GET, []() {
    http_OTA_server.sendHeader("Connection", "close");
    http_OTA_server.send(200, "text/html", loginIndex);
  });
  http_OTA_server.on("/serverIndex", HTTP_GET, []() {
    http_OTA_server.sendHeader("Connection", "close");
    http_OTA_server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  http_OTA_server.on("/update", HTTP_POST, []() {
    http_OTA_server.sendHeader("Connection", "close");
    http_OTA_server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = http_OTA_server.upload();
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

 
  //--------------------GPIO -----------------------------------------------------------------------------



  pinMode(Relais_1_GPIO,OUTPUT);
  pinMode(Relais_2_GPIO,OUTPUT);



 // ------- Statfull Switch 1-----------------------
  
 
  // Statefull ON
 /* 
  
  http_server.on ( "/relais1on", []() {
    digitalWrite(Relais_1_GPIO,HIGH);
    html_header(); 
    http_server.send ( 200,"text/html", "1" );
  } );
  
  // Statefull OFF
  http_server.on ( "/relais1off", []() {
    digitalWrite(Relais_1_GPIO,LOW);
    html_header();
    http_server.send ( 200,"text/html", "0" );
  } ); 

*/

 // ------- Statfull Switch 2-----------------------
  
 
  // Statefull ON
 /* http_server.on ( "/relais2on", []() {
    digitalWrite(Relais_2_GPIO,HIGH);
    html_header(); 
    http_server.send ( 200,"text/html", "1" );
  } );
  
  // Statefull OFF
  http_server.on ( "/relais2off", []() {
    digitalWrite(Relais_2_GPIO,LOW);
    html_header();
    http_server.send ( 200,"text/html", "0" );
  } ); 

*/
// ----------GPIO Status ----------------------------------------------------------------------------

  
  // Status 1
  http_server.on ( "/relais1status", []() {
    String stat = String(digitalRead(Relais_1_GPIO));
    //Serial.println(stat);
    html_header();
    http_server.send ( 200,"text/html", stat );
  } ); 


// Status 2
  http_server.on ( "/relais2status", []() {
    String stat = String(digitalRead(Relais_2_GPIO));
    html_header();
    http_server.send ( 200,"text/html", stat );
  } ); 


// -----Statless Switch ---------------------------------------------------------------------------------


  // Stateless 1
  http_server.on ( "/relais1", []() {
    Relais_1_Stateless = true;
    Stateless_Time1 = millis();
    digitalWrite(Relais_1_GPIO,HIGH);
    html_header();
    http_server.send ( 200,"text/html", "1" );
  } );



  // Stateless 2
  http_server.on ( "/relais2", []() {
    Relais_2_Stateless = true;
    Stateless_Time2 = millis();
    digitalWrite(Relais_2_GPIO,HIGH);
    html_header();
    http_server.send ( 200,"text/html", "1" );
  } );


  // Stateless 1 und 2
  http_server.on ( "/relais12", []() {
    Relais_1_Stateless = true;
    Relais_2_Stateless = true;
    Stateless_Time1 = millis();
    Stateless_Time2 = millis();
    digitalWrite(Relais_1_GPIO,HIGH);
    digitalWrite(Relais_2_GPIO,HIGH);
    html_header();
    http_server.send ( 200,"text/html", "1" );
  } );



// ----------Start Web Server ----------------------------------------------------------------------------
  
  http_server.begin();

  http_OTA_server.begin();
}


void loop() {


// -------- Autorestart ----------


if(!ping_start(Ping_IP, 1, 0, 0, 5)){
                Ping_check++;
                Serial.println(Ping_check); 
}
else
{
  Serial.println("Ping!");
  if(Ping_check > 5){
    Ping_check = 0;
  }
}

if (Ping_check > 10) {
                Ping_check = 0;
Serial.println("Restart!");
 delay(1000);

ESP.restart();
}




//  ------------Stateless Loop --------------------

 if ( Relais_1_Stateless) { 
    if (Stateless_Time1 + StatelessInt  < millis()) { 
      Stateless_Time1 = millis();
      digitalWrite(Relais_1_GPIO,LOW);
      Relais_1_Stateless = false;
    }
  }


if ( Relais_2_Stateless) { 
    if (Stateless_Time2 + StatelessInt  < millis()) { 
      Stateless_Time2 = millis();
      digitalWrite(Relais_2_GPIO,LOW);
      Relais_2_Stateless = false;
    }
  }

  
  // put your main code here, to run repeatedly:


  
  http_server.handleClient();
  http_OTA_server.handleClient();
  
  delay(150);
}
