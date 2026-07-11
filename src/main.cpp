#include <us/HttpServer.hpp>
#include <us/HttpClient.hpp>
#include <iostream>
#include <typeinfo>
#include <boost/json.hpp>
#include <boost/program_options.hpp>
#include "Conf.hpp"

using namespace pmc::net;
using namespace qing;
namespace json = boost::json;
namespace po = boost::program_options;

constexpr const char *_VER = "0.2.0";

void registerRoutes(HttpServer &server);
http::response<http::string_body> handleProxy(
		const http::request<http::string_body>& req,
		const std::unordered_map<std::string, std::string>& params);
bool is_special_header(const std::string &header);


static std::string	_ADDR;
static int		_PORT;
int main(int argc, char **argv) {
	po::options_description desc("cpp language web gateway");
	desc.add_options()
		("help,h",					"output help message")
		("version,v",					"output version message")
		("input,i", 	po::value<std::string>(),	"input a configure text file")
	;
	po::variables_map vm;
	{
		try {
			po::store(po::parse_command_line(argc, argv, desc), vm);
			po::notify(vm);
		}

		catch (const std::exception &e) {
			std::cerr << "ERROR: "  << e.what() << std::endl;
			std::cerr << "using --help to check options message" << std::endl;
			throw e;
		}
	}

	if (vm.count("help")) {
		std::cout << desc << std::endl;
		return 0;
	}

	if (!vm.count("input")) {
		std::cerr << "Please input configure file via -i"  << std::endl;
		return 0;
	}

	{
		try {

			ConfIni cf(vm["input"].as<std::string>());
			_ADDR = cf.get<std::string>("Server.address", "");
			_PORT = cf.get<int> ("Server.port", -1);
		}
		catch (ConfIni::IniConfigureFileParseException &e) {
			std::cerr << "ERROR: "  << e.what() << std::endl;
			std::cerr << "ini format input file error" << std::endl;
			return 1;
		}
	}

	if (_ADDR == "" || _PORT == -1) {
		std::cerr << "Please setup addree and port in Serve section in configure file";
		std::cerr << std::endl;
		return 0;
	}
		
	HttpServer server(_ADDR, _PORT, 4);
	registerRoutes(server);
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
/* TODO: or write url to the headers */
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

/* they are special because they point to raw request */
bool is_special_header(const std::string &header) {

	return header == "Content-Type" || header == "Content-Length"
		|| header == "Host";
}
