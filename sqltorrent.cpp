/*
Copyright 2016 BitTorrent Inc

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#if defined(WIN32) || defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#include <unistd.h>
#endif

#include "sqlite3.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <memory>
#include <cassert>
#include <libtorrent/session.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/version.hpp>

using namespace libtorrent;

namespace {

	struct context
	{
		context(char* s_path)
		: base(NULL), session(fingerprint("SL", 0, 1, 0, 0)), save_path(s_path)
		{
			settings_pack sp;
			sp.set_bool(settings_pack::allow_multiple_connections_per_ip, true);
			sp.set_int(settings_pack::local_upload_rate_limit, 100000);
			sp.set_int(settings_pack::local_download_rate_limit, 100000);
			// std::cout << "applying settings: " << li << std::endl;
			sp.set_int(settings_pack::alert_mask
        , alert::error_notification
        | alert::storage_notification
        | alert::status_notification);

			session.apply_settings(sp);
		}

		sqlite3_vfs* base;
		libtorrent::session session;

		char* save_path;
		void set_save_path(char* sp) {
			save_path = sp;
		}
		char* listen_interfaces;

		std::mutex mtx;
		std::vector<alert*> tmp_alerts;
		bool ready = false;
		void unblock(std::vector<alert*>* alerts) {
			tmp_alerts = *alerts;
			mtx.unlock();
		}
		void block_for_alert(std::vector<alert*>* alerts) {
			mtx.lock();
			*alerts = tmp_alerts;
		}
	};

	struct torrent_vfs_file
	{
		sqlite3_file base;
		libtorrent::session* session;
		libtorrent::torrent_handle torrent;
		context* context;
	};

	int vfs_close(sqlite3_file* file)
	{
		torrent_vfs_file* f = (torrent_vfs_file*)file;
		f->session->remove_torrent(f->torrent);
		return SQLITE_OK;
	}

	int vfs_write(sqlite3_file* file, const void* buffer, int iAmt, sqlite3_int64 iOfst)
	{
		return SQLITE_OK;
	}

	int vfs_truncate(sqlite3_file* file, sqlite3_int64 size)
	{
		return SQLITE_ERROR;
	}

	int vfs_sync(sqlite3_file*, int flags)
	{
		return SQLITE_OK;
	}

	int vfs_file_size(sqlite3_file* file, sqlite3_int64 *pSize)
	{
		torrent_vfs_file* f = (torrent_vfs_file*)file;
		*pSize = f->torrent.torrent_file()->total_size();
		return SQLITE_OK;
	}

	int vfs_lock(sqlite3_file*, int)
	{
		return SQLITE_OK;
	}

	int vfs_unlock(sqlite3_file*, int)
	{
		return SQLITE_OK;
	}

	int vfs_check_reserved_lock(sqlite3_file*, int *pResOut)
	{
		return SQLITE_OK;
	}

	int vfs_file_control(sqlite3_file*, int op, void *pArg)
	{
		return SQLITE_OK;
	}

	int vfs_sector_size(sqlite3_file* file)
	{
		torrent_vfs_file* f = (torrent_vfs_file*)file;
		return f->torrent.torrent_file()->piece_length();
	}

	int vfs_device_characteristics(sqlite3_file*)
	{
		return SQLITE_IOCAP_IMMUTABLE;
	}

	int vfs_read(sqlite3_file* file, void* buffer, int const iAmt, sqlite3_int64 const iOfst)
	{
		using namespace libtorrent;

		torrent_vfs_file* f = (torrent_vfs_file*)file;
		int const piece_size = vfs_sector_size(file);
		int piece_idx = int(iOfst / piece_size);
		int piece_offset = iOfst % piece_size;
		int residue = iAmt;
		std::uint8_t* b = (std::uint8_t*)buffer;

		do
		{
			f->torrent.set_piece_deadline(piece_idx, 0, torrent_handle::alert_when_available);

			for (;;) {
				std::vector<alert*> alerts;
				f->context->block_for_alert(&alerts);

				for (alert const* a : alerts) {
					if (alert_cast<read_piece_alert>(a)) {
						read_piece_alert const* pa = static_cast<read_piece_alert const*>(a);
						// std::cout <<
						// "piece: " << pa->piece <<
						// " - idx: " << piece_idx <<
						// " - piece_offset: " << piece_offset <<
						// " - piece_size: " << piece_size <<
						// " - residue: " << residue <<
						// std::endl;
						if (pa->piece == piece_idx) {
							assert(piece_offset < pa->size);
							assert(pa->size == piece_size);
							std::memcpy(b, pa->buffer.get() + piece_offset, (std::min<size_t>)(pa->size - piece_offset, residue));
							b += pa->size - piece_offset;
							residue -= pa->size - piece_offset;

							goto done;
						}
					}
				}
				// std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}

			done:

			++piece_idx;
			piece_offset = 0;
		} while (residue > 0);

		return SQLITE_OK;
	}

	sqlite3_io_methods torrent_vfs_io_methods = {
		1,
		vfs_close,
		vfs_read,
		vfs_write,
		vfs_truncate,
		vfs_sync,
		vfs_file_size,
		vfs_lock,
		vfs_unlock,
		vfs_check_reserved_lock,
		vfs_file_control,
		vfs_sector_size,
		vfs_device_characteristics
	};

	int torrent_vfs_open(sqlite3_vfs* vfs, const char *zName, sqlite3_file* file, int flags, int *pOutFlags)
	{
		using namespace libtorrent;

		assert(zName);
		context* ctx = (context*)vfs->pAppData;
		torrent_vfs_file* f = (torrent_vfs_file*)file;

		std::memset(f, 0, sizeof(torrent_vfs_file));
		f->base.pMethods = &torrent_vfs_io_methods;
		f->context = ctx;
		f->session = &ctx->session;

		// start DHT
		// f->session->add_dht_router(std::make_pair(
		// 	std::string("router.bittorrent.com"), 6881));
		// f->session->add_dht_router(std::make_pair(
		// 	std::string("router.utorrent.com"), 6881));
		// f->session->add_dht_router(std::make_pair(
		// 	std::string("router.bitcomet.com"), 6881));
		// f->session->start_dht();

		std::string torrentName = zName;
		// std::cout << "OPEN: " << torrentName << "-" << ctx->save_path << std::endl;
		// std::string torrentNameOrUrl = torrentName.substr( torrentName.find_last_of("/") + 1 );

		add_torrent_params p;

		// p.url = torrentName;
		p.save_path = ctx->save_path;

		try {
			p.ti = boost::make_shared<torrent_info>(torrentName, 0);
			f->torrent = ctx->session.add_torrent(p);
		} catch (libtorrent_exception e) {
			return SQLITE_CANTOPEN;
		}

		// I'm sad to say libtorrent has a bug where it incorrectly thinks a torrent is seeding
		// when it is in the checking_resume_data state. Wait here for the resume data to be checked
		// to avoid the bug.
		for (;;) {
			std::vector<alert*> alerts;
			f->context->block_for_alert(&alerts);
			// f->session->pop_alerts(&alerts);

			for (alert const* a : alerts) {
				// we need to wait for metadata_received_alert because it's a magnet link
				// and we need the size of the data (sqlite database) before starting a read
				//if (alert_cast<metadata_received_alert>(a)) {
				// torrent_checked_alert is only used when torrent files are used
				if (alert_cast<torrent_checked_alert>(a)) {
					goto done;
				}
				if (alert_cast<torrent_error_alert>(a)) {
					return SQLITE_CANTOPEN;
				}
			}
			// std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
		done:

		*pOutFlags |= SQLITE_OPEN_READONLY | SQLITE_OPEN_EXCLUSIVE;

		return SQLITE_OK;
	}

	int torrent_vfs_access(sqlite3_vfs* vfs, const char *zName, int flags, int *pResOut)
	{
		context* ctx = (context*)vfs->pAppData;
		int rc = ctx->base->xAccess(ctx->base, zName, flags, pResOut);
		if (rc != SQLITE_OK) return rc;
		*pResOut &= ~SQLITE_ACCESS_READWRITE;
		return SQLITE_OK;
	}

}

extern "C" {

	EXPORT context* new_context(char* save_path) {
		return new context(save_path);
	}

	EXPORT void set_ctx_save_path(context* ctx, char* save_path) {
		ctx->set_save_path(save_path);
	}

	EXPORT int sqltorrent_init(context* ctx, char* vfsName, int make_default)
	{
		static sqlite3_vfs vfs;
		if (!ctx->base)
		{
			ctx->base = sqlite3_vfs_find(nullptr);
			vfs = *ctx->base;
			vfs.zName = vfsName;
			vfs.pAppData = ctx;
			vfs.szOsFile = sizeof(torrent_vfs_file);
			vfs.xOpen = torrent_vfs_open;
			vfs.xAccess = torrent_vfs_access;
		}

		sqlite3_vfs_register(&vfs, make_default);

		return SQLITE_OK;
	}

	EXPORT session* get_session(context *ctx) {
		return &ctx->session;
	}

	EXPORT sqlite3* new_db() {
		sqlite3 *db;
		return db;
	}

	EXPORT int sqltorrent_open(
		const char *filename,   /* Database filename (UTF-8) */
		sqlite3 *db,         /* OUT: SQLite db handle */
		const char *zVfs        /* Name of VFS module to use */
	) {
		return sqlite3_open_v2(filename, &db, SQLITE_OPEN_READONLY, zVfs);
	}
	// EXPORT int sqlite3_sqltorrent_init(
	// 	sqlite3 *db,
	// 	char **pzErrMsg,
	// 	const sqlite3_api_routines *pApi)
	// {
	// 	int rc = SQLITE_OK;
	// 	SQLITE_EXTENSION_INIT2(pApi);
	//
	// 	rc = sqltorrent_init(1);
	//
	// 	return rc;
	// }

	EXPORT session* new_session() {
		return new session();
	}

	EXPORT add_torrent_params* new_add_torrent_params() {
		return new add_torrent_params();
	}
	EXPORT void set_url(add_torrent_params *p, char* data) {
		std::string str = data;
		p->url = str;
	}
	EXPORT void set_save_path(add_torrent_params *p, char* data) {
		std::string str = data;
		p->save_path = str;
	}
	EXPORT torrent_handle* add_torrent(session *ses, add_torrent_params *p) {
		libtorrent::error_code ec;
		torrent_handle th = ses->add_torrent(*p, ec);
		if (ec) {
			std::cout << ec.message() << std::endl;
		}
		return new torrent_handle(th);
	}

	EXPORT char* state_update_alert_msg(alert const* a) {
		state_update_alert const* st = static_cast<state_update_alert const*>(a);
		std::vector<torrent_status> statuses = st->status;

		for (torrent_status s : statuses) {
		  switch(s.state) {
		    case torrent_status::checking_files: return "checking";
		    case torrent_status::downloading_metadata: return "dl metadata";
		    case torrent_status::downloading: return "downloading";
		    case torrent_status::finished: return "finished";
		    case torrent_status::seeding: return "seeding";
		    case torrent_status::allocating: return "allocating";
		    case torrent_status::checking_resume_data: return "checking resume";
		    default: return "<>";
		  }
		}
	}

	EXPORT void alert_loop(context* ctx, void (*callback)(alert const* a, const char *data, const char *type)) {
		for (;;) {
			alert const* a = ctx->session.wait_for_alert(seconds(10));
			if (!a) continue;

			std::vector<alert*> alerts;
			ctx->session.pop_alerts(&alerts);

			if (alerts.size() > 0) {
				ctx->unblock(&alerts);
				for (alert const* a : alerts) {
					callback(a, a->message().c_str(), a->what());
					// std::cout << a->message() << std::endl;
					// if we receive the finished alert or an error, we're done
					// if (alert_cast<torrent_finished_alert>(a)) {
					//   goto done;
					// }
					// if (alert_cast<torrent_error_alert>(a)) {
					//   goto done;
					// }
				}
			}

			// std::this_thread::sleep_for(std::chrono::milliseconds(1000));

			// ask the session to post a state_update_alert, to update our
	    // state output for the torrent
	    // ctx->session.post_torrent_updates();
		}
		// done:
		// callback("done, shutting down");
		// std::cout << "done, shutting down" << std::endl;
	}

	EXPORT void query_torrents(session *ses, void (*callback)(const char *data, float progress, int download_rate, int upload_rate)) {
		std::vector<torrent_handle> v = ses->get_torrents();
		for(torrent_handle th: v) {
			torrent_status ts = th.status();
			callback(th.name().c_str(), ts.progress, ts.download_rate, ts.upload_rate);
		}
	}

	EXPORT int alert_error_code(alert const* a) {
		add_torrent_alert const* ata = static_cast<add_torrent_alert const*>(a);
		return ata->error.value();
	}
}
