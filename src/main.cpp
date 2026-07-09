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

/* {"url": "https://www.baidu.com", "method": "get", "payload": ""} */
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
		std::string payload = json::value_to<std::string>(obj.at("payload"));

		auto callback = [&res](const auto& ret) {
			res.body() += ret;	// TODO: BIG FILE
		};

		auto client = HttpClient();
		//client.set_header("Content-Type: application/json");
		client.set_header("Accept: text/html");
		client.set_header("User-Agent: HttpClient/1.0");
		client.set_header("Connection: keep-alive");

		if (method == "get") client.get_stream_sync(url, callback);
		else if (method == "post") client.post_stream_sync(url, payload, callback);

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

