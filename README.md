# TorrentPeek

> Distributed search engines using BitTorrent and SQLite

![](https://github.com/lmatteis/torrent-peek/blob/master/out.gif?raw=true)

# About

Distributed sites have gained much attention lately with systems such as [ZeroNet](https://zeronet.io/) and [IPFS](https://ipfs.io/), which seem to improve on older systems like [Freenet](https://freenetproject.org/).

Building search engines on top of these distributed systems is not quite feasible yet as users need to download the entire site database (usually several hundreds of gigabytes large) before running queries against it.

TorrentPeek allows you to build distributed search engines by leveraging the BitTorrent network and the SQLite database. Users do not have to wait until the site is fully downloaded before they can query it.

## How?

Site owners create an SQLite database (.db file) and create a torrent from this file. They then proceed to seed this torrent -- just like they would seed any other file.

Site users then start downloading the site torrent, but, rather than downloading pieces of the torrent in "rarest first" order, they download pieces based on the *search query* they performed.

**In other words, given a search query such as "indiana jones", TorrentPeek knows to download pieces of the torrent where data for "indiana jones" is likely to be stored at.**

Hence, results are given to users in a timely manner, without having to wait until the entire database is downloaded.

Furthermore, since search queries are just regular torrent piece downloads, a search query can be satisfied by many different peers in a swarm, effectively allowing for distributed search engines.

# Main features

Distributed torrent sites, queryable on demand by prioritizing specific piece downloads.

* Create distributed torrent sites accessible via your public key (eg. `33cwte8iwWn7uhtj9MKCs4q5Ax7B`) that are shared and kept alive using the BitTorrent network.

* Update your site using the Mutable Torrents extension ([BEP46](http://bittorrent.org/beps/bep_0046.html)) and let your users know about site updates via the DHT network.

* Build interactive sites that are queryable on demand using the [sqltorrent](https://github.com/bittorrent/sqltorrent) technique. Things like search engines, or complex browsable experiences are possible.

# Pros & cons

* Pros
  * Hosting your site is as simple as seeding a torrent. No need to buy a domain name or a hosting server. You can easily host your torrent site on your home network and let users visiting your site help you with the hosting.
  * Since you control your address (public key), which is broadcast via the DHT, it's much harder for governments and institution to block the content you're sharing.
  * Via sqltorrent you drive your users experience by letting them only downloading pieces of the torrent that are relevant to the users' interaction. Essentially you could create a search engine, and program interactions so that submissions of a search form result in queries to the underlying SQLite database - which prioritizes pieces based on the query.
* Cons
  * Read only sites for now. ZeroNet seems to have a solution to this problem but I'm not convinced yet.
  * Your site doesn't have a pretty name. Sharing your public key can be much harder than sharing the name of a DNS site.
  * No standards for doing distributed sites. Many different solutions (IPFS, ZeroNet, Freenet, etc..). TorrentPeek at least follows most of the BitTorrent specifications (using the powerful libtorrent library) and provides unique SQL querying capabilities.

# sqltorrent

By putting an SQLite database file (.db) inside a torrent, we can query its content -- by prioritizing pieces based on the SQL query -- and quickly *peek* at the content of the database without downloading it entirely.

TorrentPeek was inspired by [sqltorrent](https://github.com/bittorrent/sqltorrent).


A video is worth 1000 words: https://www.youtube.com/watch?v=EKttt8PYu5M&feature=youtu.be


# Install

This currently only works on Mac OS X.

Simply `npm install` and then run `node index.js file.torrent ~/save_folder/ 8080`, where the last one is the port of the UI it will lunch the app in. Make sure that `file.torrent` contains an SQLite database or else this wont work.
