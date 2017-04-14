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

#include <string>
#include <vector>
#include <cassert>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include "sqlite3.h"

static const char by_subreddit_stmt[] = "SELECT name, author, score, parent_id, body FROM comments WHERE subreddit = ?1 LIMIT 50 OFFSET ?2";
static const char by_author_stmt[] = "SELECT name, author, score, parent_id, body FROM comments WHERE author = ?1 LIMIT 50 OFFSET ?2";
static const char by_parent_stmt[] = "SELECT name, author, score, parent_id, body FROM comments WHERE parent_id = ?1";
static const char by_name_stmt[] = "SELECT name, author, score, parent_id, body FROM comments WHERE name = ?1";

struct comment
{
	std::string name;
	std::string author;
	std::string parent;
	std::string body;
	int score;
};

void print_comment(sqlite3_stmt* q)
{
	std::cout << sqlite3_column_text(q, 0) << " Author: " << sqlite3_column_text(q, 1) << " Score: " << sqlite3_column_int(q, 2) << " Parent: " << sqlite3_column_text(q, 3) << std::endl << sqlite3_column_text(q, 4) << std::endl << std::endl;
}

void print_comment_html(std::ostream& out, comment const& c)
{
	out << "<li>" << "<div style='font-size: small;'><a href='/c/" << c.name << "'>Permalink</a>" << " Author: <a href='/u/" << c.author << "'>" << c.author << "</a> Score: " << c.score << " <a href='/c/" << c.parent << "'>Parent</a></div>"
		<< c.body << "</li>\n";
}

void print_comments(sqlite3_stmt* q)
{
	int rc;
	do
	{
		rc = sqlite3_step(q);
		if (rc != SQLITE_ROW) continue;
		print_comment(q);
	} while (rc == SQLITE_ROW);
	sqlite3_reset(q);
}

void print_comment_tree(sqlite3_stmt* by_parent, std::string name, int depth)
{
	std::vector<comment> comments;
	sqlite3_bind_text(by_parent, 1, name.c_str(), name.size(), SQLITE_STATIC);
	int rc;
	do
	{
		rc = sqlite3_step(by_parent);
		if (rc != SQLITE_ROW) continue;
		comment c;
		c.name = (const char *)sqlite3_column_text(by_parent, 0);
		c.author = (const char *)sqlite3_column_text(by_parent, 1);
		c.score = sqlite3_column_int(by_parent, 2);
		c.parent = (const char *)sqlite3_column_text(by_parent, 3);
		c.body = (const char *)sqlite3_column_text(by_parent, 4);
		comments.push_back(c);
	} while (rc == SQLITE_ROW);
	sqlite3_reset(by_parent);

	for (auto const& c : comments)
	{
		std::cout << std::string(depth, ' ') << c.name << " Author: " << c.author << " Score: " << c.score << std::endl << std::string(depth, ' ') << c.body << std::endl << std::endl;
		print_comment_tree(by_parent, c.name, depth+1);
	}
}

void print_comment_tree_html(std::ostream& out, sqlite3_stmt* by_parent, std::string name, int depth)
{
	std::vector<comment> comments;
	sqlite3_bind_text(by_parent, 1, name.c_str(), name.size(), SQLITE_STATIC);
	int rc;
	do
	{
		rc = sqlite3_step(by_parent);
		if (rc != SQLITE_ROW) continue;
		comment c;
		c.name = (const char *)sqlite3_column_text(by_parent, 0);
		c.author = (const char *)sqlite3_column_text(by_parent, 1);
		c.score = sqlite3_column_int(by_parent, 2);
		c.parent = (const char *)sqlite3_column_text(by_parent, 3);
		c.body = (const char *)sqlite3_column_text(by_parent, 4);
		comments.push_back(c);
	} while (rc == SQLITE_ROW);
	sqlite3_reset(by_parent);

	if (!comments.empty())
	{
		out << "<ul>\n";
		for (auto const& c : comments)
		{
			print_comment_html(out, c);
			print_comment_tree_html(out, by_parent, c.name, depth+1);
		}
		out << "</ul>\n";
	}
}

void print_comments_html(std::ostream& out, sqlite3_stmt* q)
{
	out << "<ul>\n";
	int rc;
	do
	{
		rc = sqlite3_step(q);
		if (rc != SQLITE_ROW) continue;
		comment c;
		c.name = (const char *)sqlite3_column_text(q, 0);
		c.author = (const char *)sqlite3_column_text(q, 1);
		c.score = sqlite3_column_int(q, 2);
		c.parent = (const char *)sqlite3_column_text(q, 3);
		c.body = (const char *)sqlite3_column_text(q, 4);
		print_comment_html(out, c);
	} while (rc == SQLITE_ROW);
	sqlite3_reset(q);
	out << "</ul>\n";
}


int main()
{
	std::remove("data.sqlite3");

	// loading VFS extensions at runtime is a little awkward
	// we need a database connection to load the extension but we
	// can't open the database with the extended VFS until it is loaded
	// as a kludge open an in-memory database and use it to load the
	// extension
	sqlite3* dummy_db;
	int rc = sqlite3_open(":memory:", &dummy_db);
	assert(rc == SQLITE_OK);
	rc = sqlite3_enable_load_extension(dummy_db, 1);
	assert(rc == SQLITE_OK);
	char* err_msg;
	rc = sqlite3_load_extension(dummy_db, "sqltorrent.dll", "sqlite3_sqltorrent_init", &err_msg);
	if (rc != SQLITE_OK)
	{
		std::cerr << "Error loading sqltorrent extension: " << err_msg << std::endl;
		return -1;
	}

	sqlite3* db;
	rc = sqlite3_open_v2("data.sqlite3.torrent", &db, SQLITE_OPEN_READONLY, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_stmt* by_subreddit;
	rc = sqlite3_prepare_v2(db, by_subreddit_stmt, -1, &by_subreddit, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_stmt* by_author;
	rc = sqlite3_prepare_v2(db, by_author_stmt, -1, &by_author, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_stmt* by_parent;
	rc = sqlite3_prepare_v2(db, by_parent_stmt, -1, &by_parent, NULL);
	assert(rc == SQLITE_OK);

	sqlite3_stmt* by_name;
	rc = sqlite3_prepare_v2(db, by_name_stmt, -1, &by_name, NULL);
	assert(rc == SQLITE_OK);

#if 0
	namespace http = boost::network::http;

	struct handler;
	typedef http::server<handler> http_server;

	struct handler
	{
		void operator() (http_server::request const &request,
			http_server::response &response)
		{
			std::vector<std::string> parts;
			boost::split(parts, request.destination, boost::is_any_of("/"));
			std::stringstream out;

			out << "<html>\n";
			
			if (parts[1] == "r")
			{
				out << "<body>\n";
				sqlite3_bind_text(by_subreddit, 1, parts[2].c_str(), parts[2].size(), SQLITE_STATIC);
				sqlite3_bind_int(by_subreddit, 2, 0);
				print_comments_html(out, by_subreddit);
				out << "</body>\n";
			}
			else if (parts[1] == "u")
			{
				out << "<body>\n";
				sqlite3_bind_text(by_author, 1, parts[2].c_str(), parts[2].size(), SQLITE_STATIC);
				sqlite3_bind_int(by_author, 2, 0);
				print_comments_html(out, by_author);
				out << "</body>\n";
			}
			else if (parts[1] == "c")
			{
				sqlite3_bind_text(by_name, 1, parts[2].c_str(), parts[2].size(), SQLITE_STATIC);
				int rc = sqlite3_step(by_name);
				assert(rc = SQLITE_ROW);
				out << "<body><ul>";
				comment c;
				c.name = (const char *)sqlite3_column_text(by_name, 0);
				c.author = (const char *)sqlite3_column_text(by_name, 1);
				c.score = sqlite3_column_int(by_name, 2);
				c.parent = (const char *)sqlite3_column_text(by_name, 3);
				c.body = (const char *)sqlite3_column_text(by_name, 4);
				print_comment_html(out, c);
				sqlite3_reset(by_name);
				print_comment_tree_html(out, by_parent, parts[2], 1);
				out << "</ul></body>\n";
			}

			out << "</html>";
			
			response = http_server::response::stock_reply(http_server::response::ok, out.str());
		}

		void log(http_server::string_type const& info)
		{
			std::cerr << "cpp-netlib: " << info << std::endl;
		}

		sqlite3_stmt* by_subreddit;
		sqlite3_stmt* by_author;
		sqlite3_stmt* by_parent;
		sqlite3_stmt* by_name;
	};

	handler handler_;
	handler_.by_subreddit = by_subreddit;
	handler_.by_author = by_author;
	handler_.by_parent = by_parent;
	handler_.by_name = by_name;
	http_server::options options(handler_);
	http_server server_(options.address("127.0.0.1").port("8080"));
	server_.run();
#endif

	std::string cmd;
	do
	{
		std::cout << "$ ";
		std::cin >> cmd;
		std::vector<std::string> parts;
		boost::split(parts, cmd, boost::is_any_of("/"));

		if (parts[0] == "r")
		{
			sqlite3_bind_text(by_subreddit, 1, parts[1].c_str(), parts[1].size(), SQLITE_STATIC);
			sqlite3_bind_int(by_subreddit, 2, 0);
			print_comments(by_subreddit);
		}
		else if (parts[0] == "u")
		{
			sqlite3_bind_text(by_author, 1, parts[1].c_str(), parts[1].size(), SQLITE_STATIC);
			sqlite3_bind_int(by_author, 2, 0);
			print_comments(by_author);
		}
		else if (parts[0] == "c")
		{
			sqlite3_bind_text(by_name, 1, parts[1].c_str(), parts[1].size(), SQLITE_STATIC);
			rc = sqlite3_step(by_name);
			assert(rc = SQLITE_ROW);
			print_comment(by_name);
			sqlite3_reset(by_name);
			print_comment_tree(by_parent, parts[1], 1);
		}

	} while (cmd != "quit");

	sqlite3_close(db);
	sqlite3_close(dummy_db);
}
