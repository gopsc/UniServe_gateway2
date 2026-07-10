#include <us/HttpServer.hpp>
#include <us/HttpClient.hpp>
#include <iostream>
#include <typeinfo>
#include <boost/json.hpp>
using namespace pmc::net;
namespace json = boost::json;
void registerRoutes(HttpServer &server);
http::response<http::string_body> handleProxy(
		const http::request<http::string_body>& req,
		const std::unordered_map<std::string, std::string>& params);
bool is_special_header(const std::string &header);
int main() {
	HttpServer server("0.0.0.0", 9201, 4);
	registerRoutes(server);
	server.showStatus();
	server.start();
	server.run();
}

void registerRoutes(HttpServer &server) {

	server.post("/proxy", [](const auto& req, const auto& params) {
		HttpServer::debug::print_all_header_fields(req);
		return handleProxy(req, params);
	});
}

/* {"url": "https://www.baidu.com", "method": "get"} */
/* {"url": "https://www.baidu.com", "method": "post", "payload": ""} */
http::response<http::string_body> handleProxy(
	const http::request<http::string_body>& req,
	const std::unordered_map<std::string, std::string>& params) {
	http::response<http::string_body> res;
	res.version(req.version());

	try {
		// parse URL params, to setup client
		//...

		auto jv = json::parse(req.body());	// TODO: use error_code
		auto& obj = jv.as_object();
		std::string url = json::value_to<std::string>(obj.at("url"));
		std::string method = json::value_to<std::string>(obj.at("method"));

		auto callback = [&res](const auto& ret) {
			res.body() += ret;	// TODO: too big file
		};


		auto client = HttpClient();
		for (auto const& field: req) {
			if (is_special_header(field.name_string()))
				continue;
			auto h = std::string();
			h += field.name_string();
			h += ": ";
			h += field.value();
			client.set_header(h);	// FIXME: Content-Type should changed
		}

		if (method == "get")
			client.get_stream_sync(url, callback);
	
		else if (method == "post") {
			std::string payload = json::value_to<std::string>(obj.at("payload"));
			client.post_stream_sync(url, payload, callback);
		}

		for (const auto& pair: client.get_response_headers() ) {
			res.set(pair.first, pair.second);
		}
		res.result(client.get_response_code());
		std::cout << client.get_response_code() << std::endl;
	}
	//catch

	catch (const std::exception& e) {
		const auto& ti = typeid(e);
		res.set(http::field::content_type, "text/plain");
		res.result(http::status::internal_server_error);
		res.body() = std::string("<") + ti.name() + "> " + e.what();
	}

	res.prepare_payload();
	return res;
}

bool is_special_header(const std::string &header) {

	return header == "Content-Type" || header == "Content-Length"
		|| header == "Host";
}
