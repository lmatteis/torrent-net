var ffi = require('ffi');
var ref = require('ref')

var sqlite3 = 'void' // `sqlite3` is an "opaque" type, so we don't know its layout
  , sqlite3Ptr = ref.refType(sqlite3)
  , sqlite3PtrPtr = ref.refType(sqlite3Ptr)
  , sqlite3_exec_callback = 'pointer' // TODO: use ffi.Callback when #76 is implemented
  , stringPtr = ref.refType('string');

var SQLite3 = ffi.Library('./libsqlite3.dylib', {
  'sqlite3_libversion': [ 'string', [ ] ],
  'sqlite3_open': [ 'int', [ 'string', sqlite3PtrPtr ] ],
  'sqlite3_open_v2': [ 'int', [ 'string', sqlite3PtrPtr, 'int', 'string' ] ],
  'sqlite3_close': [ 'int', [ sqlite3Ptr ] ],
  'sqlite3_changes': [ 'int', [ sqlite3Ptr ]],
  'sqlite3_exec': [ 'int', [ sqlite3Ptr, 'string', sqlite3_exec_callback, 'void *', stringPtr ] ],
  'sqlite3_errmsg': [ 'string' , [ 'pointer' ]]
})

var sqltorrent = ffi.Library('sqltorrent.dylib', {
  'sqltorrent_init': [ 'int', [ 'pointer', 'int' ] ],
  'new_session': [ 'pointer', [ ] ],
  'new_add_torrent_params': [ 'pointer', [] ],
  'set_url': [ 'void', ['pointer', 'CString'] ],
  'set_save_path': [ 'void', ['pointer', 'CString'] ],
  'add_torrent': [ 'pointer', ['pointer', 'pointer'] ],
  'alert_loop': [ 'void', [ 'pointer', 'pointer', 'pointer'] ],
  'new_db': [ 'pointer', [ ] ],
  'sqltorrent_open': [ 'int', [ 'string', 'pointer', 'string'] ],
  'get_session': [ 'pointer', [ 'pointer'] ],
  'new_context': [ 'pointer', [ ] ],
  'query_torrents': [ 'void', [ 'pointer', 'pointer'] ],
});

var torrent = 'kat.torrent';

var ctx = sqltorrent.new_context();
sqltorrent.sqltorrent_init(ctx, 0);
var ses = sqltorrent.get_session(ctx);

// create a storage area for the db pointer SQLite3 gives us
var db = ref.alloc(sqlite3PtrPtr)

var open = SQLite3.sqlite3_open_v2.async(torrent, db, 1, 'torrent', (err, ret) => {
  console.log('DB OPENED', ret)
});

var callback = ffi.Callback('void', ['pointer', 'string', 'string'], (alert, msg, type) => {
  console.log(msg)
});
sqltorrent.alert_loop.async(ctx, ses, callback, () => {});


return;

// open the database object
var open = SQLite3.sqlite3_open_v2.async(torrent, db, 1, 'torrent', (err, ret) => {
  console.log('DB OPENED', ret)
  console.time('torrent')
  // we don't care about the `sqlite **`, but rather the `sqlite *` that it's
  // pointing to, so we must deref()
  db = db.deref()

  var rowCount = 0
  var callback2 = ffi.Callback('int', ['void *', 'int', stringPtr, stringPtr], function (tmp, cols, argv, colv) {
    var obj = {}

    for (var i = 0; i < cols; i++) {
      var colName = colv.deref()
      var colData = argv.deref()
      obj[colName] = colData
    }

    console.log(obj)
    // if (ws)
    //   ws.send(JSON.stringify(obj))

    rowCount++

    return 0
  })

  var b = new Buffer('test')
  SQLite3.sqlite3_exec.async(db, 'SELECT name FROM torrents_fts5 WHERE torrents_fts5 MATCH "sex" limit 10;', callback2, b, null, function (err, ret) {
    if (err) throw err
    if (ret !== 0) return console.log('error:', SQLite3.sqlite3_errmsg(db))

    console.log('ok', ret)
    console.timeEnd('torrent')
    // console.log('Closing...')
    // SQLite3.sqlite3_close(db)
    // fs.unlinkSync(dbName)
    // fin = true
  })
})


// var WebSocketServer = require('ws').Server;
// var express = require('express');
// var path = require('path');
// var app = express();
// var server = require('http').createServer();
//
// app.use(express.static(path.join(__dirname, '/public')));
//
// var wss = new WebSocketServer({server: server});
// var ws = null
// wss.on('connection', function (_ws) {
//   ws = _ws;
//   console.log('new connection');
//   ws.on('close', function () {
//     console.log('connection close');
//   });
//
//   ws.on('message', function incoming(message) {
//     onMessage(message)
//   });
// });
//
// server.on('request', app);
// server.listen(8080, function () {
//   console.log('Listening on http://localhost:8080');
// });

// query torrents for progress
var torrents_callback = ffi.Callback('void', ['string', 'float'], (name, progress) => {
  console.log(name + ' - ' + progress);
});
sqltorrent.query_torrents(ses, torrents_callback);
setInterval(() => {
  sqltorrent.query_torrents(ses, torrents_callback);
}, 5000);



// function onMessage(query) {
//   if(ws)
//     ws.send('exec: ' + query)
// }

// SQLite3.sqlite3_close(db)
// process.on('SIGINT', () => {
//   console.log('faaa')
//   SQLite3.sqlite3_close(db)
//   process.exit();
// });
//
// process.on('uncaughtException', function (err) {
//   console.error(err);
//   console.log("Node NOT Exiting...");
// });


// var db = sqltorrent.new_db();
// var open = sqltorrent.sqltorrent_open(torrent, db, 'torrent')
// SQLite3.sqlite3_close(db)

// var session = sqltorrent.new_session();
// var atp = sqltorrent.new_add_torrent_params();
// sqltorrent.set_url(atp, torrent);
// sqltorrent.set_save_path(atp, '.')
//
// sqltorrent.add_torrent(session, atp);
//
// var callback = ffi.Callback('void', ['string'], msg => console.log(msg));
// sqltorrent.alert_loop.async(session, callback, () => {});
