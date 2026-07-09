#include <us/HttpServer.hpp>
#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
using namespace pmc::net;
void registerRoutes(HttpServer &server);
http::response<http::string_body> handleProxy(
		const http::request<http::string_body>& req,
		const std::unordered_map<std::string, std::string>& params);
std::string buildJsonResponse(bool success, const std::string& message,
				const boost::property_tree::ptree& data = boost::property_tree::ptree());
int main() {
	HttpServer server("127.0.0.1", 9201, 4);
	registerRoutes(server);
	server.showStatus();
	server.start();
	server.run();
}

void registerRoutes(HttpServer &server) {

	server.get("/", [](const auto& req, const auto& params) {
		HttpServer::debug::print_all_header_fields(req);
		return handleProxy(req, params);
	});
}

http::response<http::string_body> handleProxy(
	const http::request<http::string_body>& req,
	const std::unordered_map<std::string, std::string>& params) {

	http::response<http::string_body> res;
	res.version(req.version());
	res.set(http::field::content_type, "application/json");

	try {
		//...
		res.result(http::status::ok);
		res.body() = buildJsonResponse(true, "Process list retrieved successfully");//, data);
	}

	catch (const std::exception& e) {
		res.result(http::status::internal_server_error);
		res.body() = buildJsonResponse(false, std::string("Error: ") + e.what());
	}

	res.prepare_payload();
	return res;
}


std::string buildJsonResponse(bool success, const std::string& message,
				const boost::property_tree::ptree& data) {
	boost::property_tree::ptree response;
	response.put("success", success);
	response.put("message", message);
	if (!data.empty()) {
		response.add_child("data", data);
	}

	std::stringstream ss;
	boost::property_tree::write_json(ss, response);
	return ss.str();
}
