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

#include <fstream>
#include <cassert>
#include <sstream>
#include <iostream>
#include <set>
#include "sqlite3.h"
#include "json/json.h"

const static char schema[] =
	"PRAGMA page_size = 32768;"
	"PRAGMA journal_mode = OFF;"
	"PRAGMA synchronous = OFF;"
	"CREATE TABLE comments ("
	"name TEXT PRIMARY KEY NOT NULL,"
	"link_id TEXT NOT NULL,"
	"created_utc INTEGER NOT NULL,"
	"ups INTEGER NOT NULL,"
	"downs INTEGER NOT NULL,"
	"gilded INTEGER NOT NULL,"
	"author TEXT NOT NULL,"
	"score INTEGER NOT NULL,"
	"subreddit TEXT NOT NULL,"
	"subreddit_id TEXT NOT NULL,"
	"parent_id TEXT,"
	"author_flair TEXT,"
	"body TEXT NOT NULL);";

const static char insert_stmt[] = "INSERT INTO comments VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13)";

static void bind_text(sqlite3_stmt* s, int idx, Json::Value& root, std::string name, Json::Value def = "none")
{
	std::string tests = root.get(name, def).asString();
	sqlite3_bind_text(s, idx, tests.data(), tests.size(), SQLITE_TRANSIENT);
}

std::set<sqlite3_int64> read_pages;
extern "C" void count_page_read(sqlite3_int64 pagenum)
{
	read_pages.insert(pagenum);
}

int callback(void* a, int b, char** c, char** d)
{
	return 0;
}

int main()
{
	std::ifstream in("data/data.json");
	sqlite3* db;

//	std::remove("data.sqlite3");

	sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);

	int rc;
	rc = sqlite3_open("data.sqlite3", &db);
	assert(rc == SQLITE_OK);

	rc = sqlite3_exec(db, schema, NULL, NULL, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_stmt* insert;
	rc = sqlite3_prepare_v2(db, insert_stmt, -1, &insert, NULL);
	while (!in.eof())
	{
		Json::Value root;
		char comment[512000];
		in.getline(comment, 512000);
		size_t clen = std::strlen(comment);
		//comment[clen++] = '}';
		//comment[clen] = 0;
		char* cstart = comment;
		while (cstart != comment + clen && *cstart != '{') ++cstart;
		if (cstart == comment + clen) continue;
		//std::cout << cstart << std::endl;
		std::stringstream cstream(cstart);
		cstream >> root;
		assert(root.type() == Json::ValueType::objectValue);
		bind_text(insert, 1, root, "name", Json::nullValue);
		bind_text(insert, 2, root, "link_id");
		bind_text(insert, 3, root, "created_utc");
		sqlite3_bind_int64(insert, 4, root.get("ups", 0).asInt64());
		sqlite3_bind_int64(insert, 5, root.get("downs", 0).asInt64());
		sqlite3_bind_int64(insert, 6, root.get("gilded", 0).asInt64());
		bind_text(insert, 7, root, "author");
		sqlite3_bind_int64(insert, 8, root.get("score", 0).asInt64());
		bind_text(insert, 9, root, "subreddit");
		bind_text(insert, 10, root, "subreddit_id");
		bind_text(insert, 11, root, "parent_id", Json::nullValue);
		bind_text(insert, 12, root, "author_flair_text", Json::nullValue);
		bind_text(insert, 13, root, "body");
		rc = sqlite3_step(insert);
		assert(rc == SQLITE_DONE);
		sqlite3_reset(insert);
	}

	sqlite3_finalize(insert);

	sqlite3_close(db);
}
