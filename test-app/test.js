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
		connection.destroy();
	});
}).listen(1584, function () {
	console.log("TCP server running at localhost:1584");
});
