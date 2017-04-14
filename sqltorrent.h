#ifndef SQLTORRENT_H_INCLUDED
#define SQLTORRENT_H_INCLUDED

#if defined(_WIN32) && !defined(SQLITE_CORE)
__declspec(dllimport)
#endif
int sqltorrent_init(int make_default);

#endif
