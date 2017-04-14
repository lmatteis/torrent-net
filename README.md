# TorrentPeek

By putting an SQLite database file (.db) inside a torrent, we can query its content -- by prioritizing pieces based on the SQL query -- and quickly *peek* at the content of the database without downloading it entirely.

TorrentPeek was inspired by [sqltorrent](https://github.com/bittorrent/sqltorrent).

A video is worth 1000 words: https://www.youtube.com/watch?v=EKttt8PYu5M&feature=youtu.be


# Install

This currently only works on Mac OS X.

Simply `npm install` and then run `node index.js file.torrent ~/save_folder/ 8080`, where the last one is the port of the UI it will lunch the app in. Make sure that `file.torrent` contains an SQLite database or else this wont work.
