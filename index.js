var ffi = require('ffi');
var ref = require('ref')
var readline = require('readline');

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
  'sqltorrent_init': [ 'int', [ 'pointer', 'string', 'int' ] ],
  'new_session': [ 'pointer', [ ] ],
  'new_add_torrent_params': [ 'pointer', [] ],
  'set_url': [ 'void', ['pointer', 'CString'] ],
  'set_save_path': [ 'void', ['pointer', 'CString'] ],
  'add_torrent': [ 'pointer', ['pointer', 'pointer'] ],
  'alert_loop': [ 'void', [ 'pointer', 'pointer'] ],
  'new_db': [ 'pointer', [ ] ],
  'sqltorrent_open': [ 'int', [ 'string', 'pointer', 'string'] ],
  'get_session': [ 'pointer', [ 'pointer'] ],
  'new_context': [ 'pointer', [ 'string'] ],
  'query_torrents': [ 'void', [ 'pointer', 'pointer'] ],
  'alert_error_code': ['int', ['pointer']],
  'state_update_alert_msg': ['string', ['pointer']],
});

if (process.argv.length < 4) {
  return console.log('pleae provide torrent file path, save path and port')
}

var torrent = process.argv[2];
var save_path = process.argv[3];
var port = process.argv[4]

// TORRENT

console.log('Opening torrent...')

var stdout = {
  progress: '',
  alert: '',
}

var ctx = sqltorrent.new_context(save_path);
sqltorrent.sqltorrent_init(ctx, 'torrent', 0); // gonna register a vfs called torrent

var callback = ffi.Callback('void', ['pointer', 'string', 'string'], (alert, msg, type) => {
  if (type === 'read_piece_alert')
    wsSend({
      alert: type + ' - ' + msg
    });
});
sqltorrent.alert_loop.async(ctx, callback, () => {});

// create a storage area for the db pointer SQLite3 gives us
var db = ref.alloc(sqlite3PtrPtr)
var ready = false;
var open = SQLite3.sqlite3_open_v2.async(torrent, db, 1, 'torrent', (err, ret) => {
  ready = true;
  wsSend({
    ready: true,
  })
  if (ret !== 0) return console.error('error:', SQLite3.sqlite3_errmsg(db))
});


function runQuery(db, query, callback) {
  var rowCount = 0
  var callback2 = ffi.Callback('int', ['void *', 'int', stringPtr, stringPtr], function (tmp, cols, argv, colv) {
    var obj = {}

    for (var i = 0; i < cols; i++) {
      var colName = ref.get(colv, ref.sizeof.pointer * i, ref.types.CString);
      var colData = ref.get(argv, ref.sizeof.pointer * i, ref.types.CString);
      obj[colName] = colData
    }

    callback(obj);

    return 0
  })

  var b = new Buffer('test')
  SQLite3.sqlite3_exec.async(db, query, callback2, b, null, function (err, ret) {
    if (err || ret !== 0)
      return wsSend({
        result: 'error: ' +  SQLite3.sqlite3_errmsg(db)
      })
  })
}

var torrents_callback = ffi.Callback('void', ['string', 'float', 'int', 'int'], (name, progress, download_rate, upload_rate) => {
  wsSend({
    progress: name + ' - %' + (progress * 100) + ' - down: ' + (download_rate / 1000) + 'kb/s - up: '  + (upload_rate / 1000) + 'kb/s',
    ready: ready,
  })

});
var ses = sqltorrent.get_session(ctx);
sqltorrent.query_torrents(ses, torrents_callback);
setInterval(() => {
  sqltorrent.query_torrents(ses, torrents_callback);
}, 1000);


// WEBSOCKET

var WebSocketServer = require('ws').Server;
var express = require('express');
var path = require('path');
var app = express();
var server = require('http').createServer();

app.use(express.static(path.join(__dirname, '/public')));

var wss = new WebSocketServer({server: server });
var ws = null
wss.on('connection', function (_ws) {
  ws = _ws;
  ws.on('close', function () {
  });

  ws.on('message', function incoming(message) {
    runQuery(db.deref(), message, response => {
      wsSend({
        result: response
      })
    })
  });
});

server.on('request', app);
server.listen(port, function () {
  console.log('Listening on http://localhost:'+ port);
  var spawn = require('child_process').spawn
  spawn('open', ['http://localhost:'+port]);
});

function wsSend(str) {
  if (ws) {
    try {
      ws.send(JSON.stringify(str));
    } catch (e) {
    }
  }
}
