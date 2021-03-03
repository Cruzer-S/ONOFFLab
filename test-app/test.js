const net = require('net');
const fs = require('fs');

let tcp = net.createServer(connection => {
	console.log("new connection: " + connection.remoteAddress);

	connection.setTimeout(1000);

	connection.on('data', data => {

		fs.writeFile('data.bin', data, 'binary', function (err) {
			if (err) {
				console.log(err);
			}
		});
	});

	connection.on('close', () => {
		console.log("client disconnected " + connection.remoteAddress);

		device.splice(device.indexOf(connection), 1);
		connection.destroy();
	});

	device.push(connection);
}).listen(1584, "0.0.0.0", function () {
	console.log("TCP server running at 0.0.0.0:1584");
});
