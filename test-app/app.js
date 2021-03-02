var firebase = require('firebase').initializeApp({
    serviceAccount: "./pigdonsa-firebase-adminsdk-ipyj0-1ed21ab20e.json",
    apiKey: "AIzaSyB1gOanVKthzDg-G9gRFkuX7z1cXrp2MWM",
    authDomain: "pigdonsa.firebaseapp.com",
    databaseURL: "https://pigdonsa.firebaseio.com",
})
    //about the Firebase
var firemain = firebase.database().ref();
var querystring = require('querystring');
const mqtt = require('mqtt');
var express = require("express");
var bodyParser = require('body-parser');
var http = require('http');
var path = require('path');
var app = express();
var request=require('request');

app.all('*', function(req, res, next) {
	res.header("Access-Control-Allow-Origin", "*");
	res.header("Access-Control-Allow-Headers", "X-Requested-With");
	res.header("Access-Control-Allow-Methods","PUT,POST,GET,DELETE,OPTIONS");
	res.header("X-Powered-By",' 3.2.1');
	res.header("Content-Type", "application/json;charset=utf-8");
	next();
});
app.use(express.static(path.join(__dirname, '/')));
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));
// app.use(http);


// mqtt cloud code commented out.


// var options = {
//     port: 10787,
//     host: 'mqtt://soldier.cloudmqtt.com',
//     clientId: 'mqttjs_' + Math.random().toString(16).substr(2, 8),
//     username: 'jzqpjtfy',
//     password: 'pY9bvfNeSm8A',
//     keepalive: 60,
//     reconnectPeriod: 1000,
//     protocolId: 'MQTT',
//     protocolVersion: 4,
//     clean: true,
//     encoding: 'utf8'
//   };
//   var led ="";
//   var client = mqtt.connect('mqtt://soldier.cloudmqtt.com',options);
//   client.on('connect', function() { // When connectedf
//     console.log('connected');


//     client.subscribe('/ESP8266/disconnect', function() {
//         // when a message arrives, do something with it
//         client.on('message', function(topic, message, packet) {
//             console.log("Received '" + message + "' on '" + topic + "'");
//         });
//     });
//     // subscribe to a topic
//     client.subscribe('/ESP8266/pressed', function() {
//         // when a message arrives, do something with it
//         client.on('message', function(topic, message, packet) {
//             console.log("Received '" + message + "' on '" + topic + "'");
//         });
//     });

//     client.subscribe('/ESP8266/pressed2', function() {
//         // when a message arrives, do something with it
//         client.on('message', function(topic, message, packet) {
//             console.log("Received '" + message + "' on '" + topic + "'");
//         });
//     });

//     client.subscribe('/ESP8266/pressed3', function() {
//         // when a message arrives, do something with it
//         client.on('message', function(topic, message, packet) {
//             console.log("Received '" + message + "' on '" + topic + "'");
//         });
//     });
//     console.log("led status:"+led);

//   });

app.set('views', './views');
app.set('view engine', 'ejs');

app.get('/', function(req, res) {
    console.log("goto login page")
    res.render('login', {'flag':''});
});


var server = app.listen(process.env.PORT || 52275, function() {
    var port = server.address().port;

    console.log("Express is working on port " + port);

    setInterval(() => {

    }, 10000);
});
