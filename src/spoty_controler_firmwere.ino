
#include <base64.h>
#include <ArduinoJson.h>
#include <stdint.h>
#include <stdio.h>

// Include WiFi and http client
#include <ESP8266WiFi.h> 
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiClientSecureBearSSL.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <WiFiNINA.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// WIFI credentials
char WIFI_SSID[] = "CALCALZON_2.5G"
char PASSWORD[] = "Ser211@TeleCentro"

// Spotify API credentials
#define CLIENT_ID "7dbaacd4aa4d4f668e6782175ef4e4b4"
#define CLIENT_SECRET "32ad905e011046e59976f91aaf5ba254"
String REDIRECT_URI = "http://192.168.0.12/callback";

#define codeVersion "1.2.0"

#if defined(ARDUINO) && ARDUINO >= 100
#define printByte(args)  write(args)
#else
#define printByte(args)  print(args,BYTE)
#endif

uint8_t play[8]  = {0x02,0x06,0x0E,0x1E,0x1E,0x0E,0x06,0x02};
uint8_t pause[8]  = {0x00,0x1B,0x1B,0x1B,0x1B,0x1B,0x1B,0x00};
uint8_t heart[8] = {0x00,0x00,0x0A,0x1F,0x1F,0x0E,0x04,0x00};
uint8_t heartE[8]  = {0x00,0x00,0x0A,0x15,0x11,0x0A,0x04,0x00};
uint8_t endF[8] = {0x00,0x08,0x08,0x1B,0x08,0x08,0x00,0x00};
uint8_t endL[8] = {0x00,0x02,0x0A,0x1E,0x0A,0x02,0x00,0x00};
uint8_t point[8] = {0x00,0x00,0x0E,0x1F,0x0E,0x00,0x00,0x00};
uint8_t starter[8] = {0x00,0x08,0x08,0x0F,0x08,0x08,0x00,0x00};

IPAddress local_IP(192, 168, 1, 180);
// Set your Gateway IP address
IPAddress gateway(192, 168, 1, 1);

IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional
  
LiquidCrystal_I2C lcd(0x3F,20,4);  

String getValue(HTTPClient &http, String key) {
  bool found = false, look = false, seek = true;
  int ind = 0;
  String ret_str = "";

  int len = http.getSize();
  char char_buff[1];
  WiFiClient * stream = http.getStreamPtr();
  while (http.connected() && (len > 0 || len == -1)) {
    size_t size = stream->available();
    // Serial.print("Size: ");
    // Serial.println(size);
    if (size) {
      int c = stream->readBytes(char_buff, ((size > sizeof(char_buff)) ? sizeof(char_buff) : size));
      if (found) {
        if (seek && char_buff[0] != ':') {
          continue;
        } else if(char_buff[0] != '\n'){
            if(seek && char_buff[0] == ':'){
                seek = false;
                int c = stream->readBytes(char_buff, 1);
            }else{
                ret_str += char_buff[0];
            }
        }else{
            break;
        }
          
        // Serial.print("get: ");
        // Serial.println(get);
      }
      else if ((!look) && (char_buff[0] == key[0])) {
        look = true;
        ind = 1;
      } else if (look && (char_buff[0] == key[ind])) {
        ind ++;
        if (ind == key.length()) found = true;
      } else if (look && (char_buff[0] != key[ind])) {
        ind = 0;
        look = false;
      }
    }
  }
//   Serial.println(*(ret_str.end()));
//   Serial.println(*(ret_str.end()-1));
//   Serial.println(*(ret_str.end()-2));
  if(*(ret_str.end()-1) == ','){
    ret_str = ret_str.substring(0,ret_str.length()-1);
  }
  return ret_str;
}

//http response struct
struct httpResponse{
    int responseCode;
    String responseMessage;
};

//song details struct
struct songDetails{
    int durationMs;
    String album;
    String artist;
    String song;
    String Id;
    bool isLiked;
};

//Create spotify connection class
class SpotConn {
public:
	SpotConn(){
        client = std::make_unique<BearSSL::WiFiClientSecure>();
        client->setInsecure();
    }
    // httpResponse makeSpotifyRequest(const char* URI, const char** headers, int numHeaders, const char* RequestBody){
    //     https.begin(*client,URI);
    //     for(;numHeaders>0;numHeaders--,headers += 2){
    //         https.addHeader(*headers,*(headers+1));
    //     }
    //     struct httpResponse res;
    //     res.responseCode = https.POST(RequestBody);
    //     res.responseMessage = https.getString()
    //     https.end();
    //     return res;
    // }
	bool getUserCode(String serverCode) {
        https.begin(*client,"https://accounts.spotify.com/api/token");
        String auth = "Basic " + base64::encode(String(CLIENT_ID) + ":" + String(CLIENT_SECRET));
        https.addHeader("Authorization",auth);
        https.addHeader("Content-Type","application/x-www-form-urlencoded");
        String requestBody = "grant_type=authorization_code&code="+serverCode+"&redirect_uri="+String(REDIRECT_URI);
        // Send the POST request to the Spotify API
        int httpResponseCode = https.POST(requestBody);
        // Check if the request was successful
        if (httpResponseCode == HTTP_CODE_OK) {
            String response = https.getString();
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, response);
            accessToken = String((const char*)doc["access_token"]);
            refreshToken = String((const char*)doc["refresh_token"]);
            tokenExpireTime = doc["expires_in"];
            tokenStartTime = millis();
            accessTokenSet = true;
            Serial.println(accessToken);
            Serial.println(refreshToken);
        }else{
            Serial.println(https.getString());
        }
        // Disconnect from the Spotify API
        https.end();
        return accessTokenSet;
    }
    bool refreshAuth(){
        https.begin(*client,"https://accounts.spotify.com/api/token");
        String auth = "Basic " + base64::encode(String(CLIENT_ID) + ":" + String(CLIENT_SECRET));
        https.addHeader("Authorization",auth);
        https.addHeader("Content-Type","application/x-www-form-urlencoded");
        String requestBody = "grant_type=refresh_token&refresh_token="+String(refreshToken);
        // Send the POST request to the Spotify API
        int httpResponseCode = https.POST(requestBody);
        accessTokenSet = false;
        // Check if the request was successful
        if (httpResponseCode == HTTP_CODE_OK) {
            String response = https.getString();
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, response);
            accessToken = String((const char*)doc["access_token"]);
            // refreshToken = doc["refresh_token"];
            tokenExpireTime = doc["expires_in"];
            tokenStartTime = millis();
            accessTokenSet = true;
            Serial.println(accessToken);
            Serial.println(refreshToken);
        }else{
            Serial.println(https.getString());
        }
        // Disconnect from the Spotify API
        https.end();
        return accessTokenSet;
    }


    bool togglePlay(){
        String url = "https://api.spotify.com/v1/me/player/" + String(isPlaying ? "pause" : "play");
        isPlaying = !isPlaying;
        https.begin(*client,url);
        String auth = "Bearer " + String(accessToken);
        https.addHeader("Authorization",auth);
        int httpResponseCode = https.PUT("");
        bool success = false;
        // Check if the request was successful
        if (httpResponseCode == 204) {
            // String response = https.getString();
            Serial.println((isPlaying ? "Playing" : "Pausing"));
            success = true;
        } else {
            Serial.print("Error pausing or playing: ");
            Serial.println(httpResponseCode);
            String response = https.getString();
            Serial.println(response);
        }

        
        // Disconnect from the Spotify API
        https.end();
        getTrackInfo();
        return success;
    }
    bool getTrackInfo(){
        String url = "https://api.spotify.com/v1/me/player/currently-playing";
        https.useHTTP10(true);
        https.begin(*client,url);
        String auth = "Bearer " + String(accessToken);
        https.addHeader("Authorization",auth);
        int httpResponseCode = https.GET();
        bool success = false;
        String songId = "";
        bool refresh = false;
        // Check if the request was successful
        if (httpResponseCode == 200) {
                        // 

            String currentSongProgress = getValue(https,"progress_ms");
            currentSongPositionMs = currentSongProgress.toFloat();
            
            // Serial.println(imageLink);
            
            
            String albumName = getValue(https,"name");
            String artistName = getValue(https,"name");
            String songDuration = getValue(https,"duration_ms");
            currentSong.durationMs = songDuration.toInt();
            String songName = getValue(https,"name");
            songId = getValue(https,"uri");
            String isPlay = getValue(https, "is_playing");
            isPlaying = isPlay == "true";
            Serial.println(isPlay);
            // Serial.println(songId);
            songId = songId.substring(15,songId.length()-1);
            // Serial.println(songId);
            https.end();
            // listSPIFFS();
            
            currentSong.album = albumName.substring(1,albumName.length()-1);
            currentSong.artist = artistName.substring(1,artistName.length()-1);
            currentSong.song = songName.substring(1,songName.length()-1);
            currentSong.Id = songId;
            currentSong.isLiked = findLikedStatus(songId);
            success = true;
        } else {
            Serial.print("Error getting track info: ");
            Serial.println(httpResponseCode);
            // String response = https.getString();
            // Serial.println(response);
            https.end();
        }
        
        
        // Disconnect from the Spotify API
        if(success){
            lastSongPositionMs = currentSongPositionMs;
        }
        return success;
    }
    bool findLikedStatus(String songId){
        String url = "https://api.spotify.com/v1/me/tracks/contains?ids="+songId;
        https.begin(*client,url);
        String auth = "Bearer " + String(accessToken);
        https.addHeader("Authorization",auth);
        https.addHeader("Content-Type","application/json");
        int httpResponseCode = https.GET();
        bool success = false;
        // Check if the request was successful
        if (httpResponseCode == 200) {
            String response = https.getString();
            https.end();
            return(response == "[ true ]");
        } else {
            Serial.print("Error toggling liked songs: ");
            Serial.println(httpResponseCode);
            String response = https.getString();
            Serial.println(response);
            https.end();
        }

        
        // Disconnect from the Spotify API
        
        return success;
    }
    bool toggleLiked(String songId){
        String url = "https://api.spotify.com/v1/me/tracks/contains?ids="+songId;
        https.begin(*client,url);
        String auth = "Bearer " + String(accessToken);
        https.addHeader("Authorization",auth);
        https.addHeader("Content-Type","application/json");
        int httpResponseCode = https.GET();
        bool success = false;
        // Check if the request was successful
        if (httpResponseCode == 200) {
            String response = https.getString();
            https.end();
            if(response == "[ true ]"){
                currentSong.isLiked = false;
                dislikeSong(songId);
            }else{
                currentSong.isLiked = true;
                likeSong(songId);
            }
            Serial.println(response);
            success = true;
        } else {
            Serial.print("Error toggling liked songs: ");
            Serial.println(httpResponseCode);
            String response = https.getString();
            Serial.println(response);
            https.end();
        }

        
        // Disconnect from the Spotify API
        
        return success;
    }
    bool adjustVolume(int vol){
        String url = "https://api.spotify.com/v1/me/player/volume?volume_percent=" + String(vol);
        https.begin(*client,url);
        String auth = "Bearer " + String(accessToken);
        https.addHeader("Authorization",auth);
        int httpResponseCode = https.PUT("");
        bool success = false;
        // Check if the request was successful
        if (httpResponseCode == 204) {
            // String response = https.getString();
            currVol = vol;
            success = true;
        }else if(httpResponseCode == 403){
             currVol = vol;
            success = false;
            Serial.print("Error setting volume: ");
            Serial.println(httpResponseCode);
            String response = https.getString();
            Serial.println(response);
        } else {
            Serial.print("Error setting volume: ");
            Serial.println(httpResponseCode);
            String response = https.getString();
            Serial.println(response);
        }

        
        // Disconnect from the Spotify API
        https.end();
        return success;
    }
    bool skipForward(){
        String url = "https://api.spotify.com/v1/me/player/next";
        https.begin(*client,url);
        String auth = "Bearer " + String(accessToken);
        https.addHeader("Authorization",auth);
        int httpResponseCode = https.POST("");
        bool success = false;
        // Check if the request was successful
        if (httpResponseCode == 204) {
            // String response = https.getString();
            Serial.println("skipping forward");
            success = true;
        } else {
            Serial.print("Error skipping forward: ");
            Serial.println(httpResponseCode);
            String response = https.getString();
            Serial.println(response);
        }

        
        // Disconnect from the Spotify API
        https.end();
        getTrackInfo();
        return success;
    }
    bool skipBack(){
        String url = "https://api.spotify.com/v1/me/player/previous";
        https.begin(*client,url);
        String auth = "Bearer " + String(accessToken);
        https.addHeader("Authorization",auth);
        int httpResponseCode = https.POST("");
        bool success = false;
        // Check if the request was successful
        if (httpResponseCode == 204) {
            // String response = https.getString();
            Serial.println("skipping backward");
            success = true;
        } else {
            Serial.print("Error skipping backward: ");
            Serial.println(httpResponseCode);
            String response = https.getString();
            Serial.println(response);
        }

        
        // Disconnect from the Spotify API
        https.end();
        getTrackInfo();
        return success;
    }
    bool likeSong(String songId){
        String url = "https://api.spotify.com/v1/me/tracks?ids="+songId;
        https.begin(*client,url);
        String auth = "Bearer " + String(accessToken);
        https.addHeader("Authorization",auth);
        https.addHeader("Content-Type","application/json");
        char requestBody[] = "{\"ids\":[\"string\"]}";
        int httpResponseCode = https.PUT(requestBody);
        bool success = false;
        // Check if the request was successful
        if (httpResponseCode == 200) {
            // String response = https.getString();
            Serial.println("added track to liked songs");
            success = true;
        } else {
            Serial.print("Error adding to liked songs: ");
            Serial.println(httpResponseCode);
            String response = https.getString();
            Serial.println(response);
        }

        
        // Disconnect from the Spotify API
        https.end();
        return success;
    }
    bool dislikeSong(String songId){
        String url = "https://api.spotify.com/v1/me/tracks?ids="+songId;
        https.begin(*client,url);
        String auth = "Bearer " + String(accessToken);
        https.addHeader("Authorization",auth);
        // https.addHeader("Content-Type","application/json");
        // char requestBody[] = "{\"ids\":[\"string\"]}";
        int httpResponseCode = https.DELETE();
        bool success = false;
        // Check if the request was successful
        if (httpResponseCode == 200) {
            // String response = https.getString();
            Serial.println("removed liked songs");
            success = true;
        } else {
            Serial.print("Error removing from liked songs: ");
            Serial.println(httpResponseCode);
            String response = https.getString();
            Serial.println(response);
        }

        
        // Disconnect from the Spotify API
        https.end();
        return success;
    }

    bool accessTokenSet = false;
    long tokenStartTime;
    int tokenExpireTime;
    songDetails currentSong;
    float currentSongPositionMs;
    float lastSongPositionMs;
    int currVol;
    bool isPlaying = false;
  private:
    std::unique_ptr<BearSSL::WiFiClientSecure> client;
    HTTPClient https;
    String accessToken;
    String refreshToken;
};


WiFiUDP ntpUDP;
ESP8266WebServer server(80); //Server on port 80
SpotConn spotifyConnection;
NTPClient timeClient(ntpUDP, "ar.pool.ntp.org", -10800, 60000); // NTP client


class LCDmanager { // the one responsable for managing all graphical intefrace
public:
  LCDmanager(){};

  int nameScroll = 0;
  int lastScroll = 0;
  String lastSong = "";
  bool scrolable;

  void drawStart(){ // draws the start up page
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WIRELESS CONTROLLER"); // project name
    lcd.setCursor(0, 1);
    lcd.print(String("version ") + String(codeVersion)); // shows version
    lcd.setCursor(0, 2);
    lcd.print("by: Manuel Rao"); // shows author
  }

  void drawSpotifyConection(){ // draws the spotify setpu page
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Spotify Setup");
    lcd.setCursor(0, 1);
    lcd.print("go to:");
    lcd.setCursor(0, 2);
    lcd.print("http://" + WiFi.localIP().toString() + "/"); // shows the URI you need to connect to to auth
    lcd.setCursor(0, 3);
    lcd.print("To sync to spotify");
  }

  void waitForDevice(){ // draws the waiting for device page
    lcd.clear();
    lcd.setCursor(15, 0);
    timeClient.update();
    lcd.print(timeClient.getFormattedTime().substring(0,5));
    lcd.setCursor(0,1);
    lcd.print("waiting for device");
  }


  void showNets(int numNets, int selected){ // it shows all networks
    
    String IDs[numNets] = [];
    for(int i = 0; i < numNets; i++){
        IDs[i] = WiFi.SSID(i)
    }

    Serial.println(IDs);
    
    int showNum = int(selected/2);

    if(numNets%2 != 0){
        IDs[numNets+1] = "";
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Networks - " + String(numNets)); // menu name and number of networks
    lcd.setCursor(0, 1);
    lcd.print(String(((selected % 2) == 0)? "- " : "->") + IDs[showNum]); // shows first network
    lcd.setCursor(0, 2);
    lcd.print(String(((selected % 2) != 0)? "- " : "->") + IDs[showNum+1]); //shows second network
    lcd.setCursor(0, 3);
    lcd.print("            < E > S"); // shows action bar
  }
    
  void drawKeyboard(String menuText, String writenText, char leter, int selected, bool writing){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("- " + menuText);
    String writeLine = "";
    scrolable = (writenText.length() >= 18) ? true : false;
    if scrolable{ // prepares the text to be shown 
        if (scrolable){
            writeLine = writenText.substring(selected-18, selected) + leter;
        } else if(writing){
            writeLine = writenText.substring(0, selected) + leter + writenText.substring(selected+1, 20)
        }
    }
    lcd.setCursor(0, 1);
    lcd.print(writeLine); // writes the actual text
    lcd.setCursor(0, 2);
    String selector = (writing) ? ">-<" : "<->"; // select which cursor to use
    if(selected >= 18){ // print blank spaces to place the cursor in the apropiate space
        for(int i = 0; i<17; i++){
            lcd.print(" ");
        }
    } else {
        for(int i = 0; i<selected-1; i++){
            lcd.print(" ");
        }
    }
    lcd.print(selector);
    lcd.setCursor(0, 3);
    lcd.print(String((writing)? "            < S > U" : "            < W > S")); //menu action bar
  }

  void drawMusic(){ // draws the main music screen

    lcd.clear();
    lcd.setCursor(15, 0);
    timeClient.update(); // shows the time
    lcd.print(timeClient.getFormattedTime().substring(0,5));

    // shows song name
    lcd.setCursor(0,1);
    int nameLen = spotifyConnection.currentSong.song.length();
    if(lastSong != spotifyConnection.currentSong.song){
      if(nameLen > 20){
        scrolable = true;
        nameScroll = 0;
      }else{
        scrolable = false;
      }
      lastSong = spotifyConnection.currentSong.song;
    }

    //scrolling
    String displayName = spotifyConnection.currentSong.song + "-("+spotifyConnection.currentSong.artist+")";
    if(scrolable){
      lcd.print(displayName.substring(nameScroll, nameScroll+20));
      int steps = int((millis()-LastUpdate)/1500);
      if(nameScroll + 20 == nameLen){nameScroll = 0;}else if(nameScroll + 20 + steps <= nameLen){nameScroll += steps;}else{nameScroll++;}
      LastUpdate = millis();
    }else{
      lcd.print(spotifyConnection.currentSong.song);
    }

    //progress bar
    lcd.setCursor(0, 2);
    float progress = float(spotifyConnection.currentSongPositionMs)/float(spotifyConnection.currentSong.durationMs);
    Serial.println(spotifyConnection.currentSongPositionMs);
    Serial.println(spotifyConnection.currentSong.durationMs);
    Serial.println(progress);
    int bars = floor(progress*18)-1;
    Serial.println(bars);
    bool l_finisher = (int(round(progress*36))%2==1)? true : false;
    lcd.printByte(2);
    for(int i = 0; i <= bars; i ++){
      lcd.printByte(7);
    }
    lcd.printByte((l_finisher)? 5 : 6);
    String bar = "";
    for(int i = 0; i < (16 - bars); i ++){
      bar = bar + "-";
    }
    lcd.print(bar);
    lcd.printByte(6);

    // show progress time and total song length
    lcd.setCursor(0, 3);
    int progressMinsIn = floor(spotifyConnection.currentSongPositionMs/60000);
    String progressMins = String((progressMinsIn < 10) ? "0" + String(progressMinsIn) : String(progressMinsIn));
    int progressSecIn = floor(spotifyConnection.currentSongPositionMs/1000) - progressMinsIn*60;
    String progressSec = String((progressSecIn < 10) ? "0" + String(progressSecIn) : String(progressSecIn));
    lcd.print(progressMins + ":" + progressSec + "/");
    int durationMinsIn = floor(spotifyConnection.currentSong.durationMs/60000);
    String durationMins = String((durationMinsIn < 10) ? "0" + String(durationMinsIn) : String(durationMinsIn));
    int durationSecIn = floor(spotifyConnection.currentSong.durationMs/1000) - durationMinsIn*60;
    String durationSec = String((durationSecIn < 10) ? "0" + String(durationSecIn) : String(durationSecIn));
    lcd.print(durationMins + ":" + durationSec);
    lcd.print("  < "); //backtrack
    spotifyConnection.isPlaying ? lcd.printByte(0) : lcd.printByte(1); //pause/play button
    lcd.print(" > "); //skip button
    spotifyConnection.currentSong.isLiked ? lcd.printByte(3) : lcd.printByte(4);
  }

  int LastUpdate = 0;
};

bool buttonStates[] = {1,1,1,1};
int debounceDelay = 20;
unsigned long debounceTimes[] = {0,0,0,0};
int buttonPins[] = {D5,D6,D7,D8};
int oldvolume = 100;
int volume = 100;

long timeLoop;
long refreshLoop;
bool serverOn = true;

LCDmanager LCDm;

//web pages
// HTML for main root page
const char mainPage[] PROGMEM = R"=====(
<HTML>
    <HEAD>
        <TITLE>My first web page</TITLE>
    </HEAD>
    <BODY>
        <CENTER>
            <B>Hello World.... </B>
            <a href="https://accounts.spotify.com/authorize?response_type=code&client_id=%s&redirect_uri=%s&scope=user-modify-playback-state user-read-currently-playing user-read-playback-state user-library-modify user-library-read">Log in to spotify</a>
        </CENTER>
    </BODY>
</HTML>
)=====";

// HTML for error page
const char errorPage[] PROGMEM = R"=====(
<HTML>
    <HEAD>
        <TITLE>My first web page</TITLE>
    </HEAD>
    <BODY>
        <CENTER>
            <B>Hello World.... </B>
            <a href="https://accounts.spotify.com/authorize?response_type=code&client_id=%s&redirect_uri=%s&scope=user-modify-playback-state user-read-currently-playing user-read-playback-state user-library-modify user-library-read">Log in to spotify</a>
        </CENTER>
    </BODY>
</HTML>
)=====";

void handleRoot() { // handless HTTP main server for spotify auth
    Serial.println("handling root");
    char page[500];
    sprintf(page,mainPage,CLIENT_ID,REDIRECT_URI);
    server.send(200, "text/html", String(page)+"\r\n"); //Send web page
}

void handleCallbackPage() { // handless call back page but it can also act as root for auth
    if(!spotifyConnection.accessTokenSet){
        if (server.arg("code") == ""){     //Parameter not found
            char page[500];
            sprintf(page,errorPage,CLIENT_ID,REDIRECT_URI);
            server.send(200, "text/html", String(page)); //Send web page
        }else{     //Parameter found
            if(spotifyConnection.getUserCode(server.arg("code"))){ // send auth complete web page
                server.send(200,"text/html","Spotify setup complete Auth refresh in :"+String(spotifyConnection.tokenExpireTime));
            }else{
                char page[500];
                sprintf(page,errorPage,CLIENT_ID,REDIRECT_URI);
                server.send(200, "text/html", String(page)); //Send error web page
            }
        }
    }else{
        server.send(200,"text/html","Spotify setup complete"); // send spotify setup already complete page
    }
}

// pin manager, its called whenever a button is pressed(DO NOT TOUCH UNLES YOU KNOW WHAT UR DOING)
volatile int pinCalled;
volatile bool call = false;
void ICACHE_RAM_ATTR pinManager(){
  call = true;
  for(int i = 0; i < 5; i ++){
    int reading = digitalRead(buttonPins[i]);
    if(reading == true && i != 4){  
      switch (i){
      case 0:
          pinCalled = 0;
          break;
      case 1:
          pinCalled = 1;
          break;
      case 2:
          pinCalled = 2;
          break;
      case 3:
          pinCalled = 3;
          break;
      
      default:
          break;
      }
    }
  }
}

void manageWifiConnection(){ // manages all wifi connection setup to be able to connect to any network type 
    int numNets = WiFi.scanNetworks();
    int selected = 0;
    bool selectedSet = false;
    bool q = false;
    while(!selectedSet and !q){
        if(call){
            switch (pinCalled){
                case 0:
                    selected = (selected == 0) ? numNets - 1 : selected - 1;
                    break;
                case 1:
                    selectedSet = true;
                    break;
                case 2:
                    selected = (selected == numNets - 1) ? 0 : selected + 1
                    break;
                case 3:
                    q = true;
                    break;
                default:
                    break;
            }
            call = false;
        }
    }
        
}

void setup(){
  Serial.begin(115200);

  lcd.init();    // initialize the lcd                   
  lcd.backlight();
  // load in the custom chars
  lcd.createChar(0, pause);
  lcd.createChar(1, play);
  lcd.createChar(2, starter);
  lcd.createChar(3, heart);
  lcd.createChar(4, heartE);
  lcd.createChar(5, endF);
  lcd.createChar(6, endL);
  lcd.createChar(7, point);
  lcd.home();
  LCDm.drawStart();



  lcd.setCursor(0, 3);
  lcd.print("connecting to WiFi");
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  
  WiFi.begin(WIFI_SSID, PASSWORD); // connect to wifi
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.println("Conecting to WiFi...");
      lcd.setCursor(19, 3);
      lcd.print(attempt);
      attempt++;

  }

  lcd.setCursor(0, 3);
  lcd.print("connected to WiFi   ");
  Serial.println("Connected to WiFi\n Ip is: ");
  Serial.println(WiFi.localIP());
  REDIRECT_URI = "http://" + WiFi.localIP().toString() + "/callback";
  delay(500);
  timeClient.begin(); // initialize the time client
  lcd.setCursor(0, 3);
  lcd.print("connected to NTPC   ");

  server.on("/", handleRoot);      //Which routine to handle at root location
  server.on("/callback", handleCallbackPage);      //Which routine to handle at root location
  server.begin();                  //Start server
  Serial.println("HTTP server started");
  lcd.setCursor(0, 3);
  lcd.print("HTTPS server Started");

  for(int i = 0; i < 4; i ++){
    pinMode(buttonPins[i], INPUT);
    attachInterrupt(digitalPinToInterrupt(buttonPins[i]), pinManager, RISING);
  }

  delay(250);
  lcd.setCursor(0, 3);
  lcd.print("Start sequence DONE ");
  delay(500);
  LCDm.drawSpotifyConection(); // wait for spotify auth

}

int mode = 0;
void loop(){
  if(spotifyConnection.accessTokenSet){
    if(serverOn){
        server.close();
        serverOn = false;
    }
    if((millis() - spotifyConnection.tokenStartTime)/1000 > spotifyConnection.tokenExpireTime){
        Serial.println("refreshing token");
        if(spotifyConnection.refreshAuth()){
            Serial.println("refreshed token");
        }
    }

    if(spotifyConnection.getTrackInfo()){
      LCDm.drawMusic();
      Serial.println("drawn");
    }else if(spotifyConnection.currentSong.song == NULL){
      LCDm.waitForDevice();
    }
    if(call){
      switch (pinCalled){
        case 0:
            spotifyConnection.skipBack();
            break;
        case 1:
            spotifyConnection.togglePlay();
            break;
        case 2:
            spotifyConnection.skipForward();
            break;
        case 3:
            spotifyConnection.toggleLiked(spotifyConnection.currentSong.Id); // need to change this to cofig menu
            break;
        default:
            break;
      }
      call = false;
    }
    Serial.println(analogRead(int(A0)));
    int volRequest = map(analogRead(A0),0,1023,0,100);
    if(abs(volRequest - spotifyConnection.currVol) > 2){
        spotifyConnection.adjustVolume(volRequest);
    } 
    timeLoop = millis();
  }else{
      server.handleClient();
  }
}
