#include <us/HttpServer.hpp>
#include <us/HttpClient.hpp>
#include <us/MyRSA.hpp>
#include <us/Base64.hpp>
#include <us/SHA256.hpp>
#include <iostream>
#include <typeinfo>
#include <memory>
#include <boost/json.hpp>
#include <boost/program_options.hpp>
#include <openssl/provider.h>
#include "Conf.hpp"

using namespace pmc::net;
using namespace qing;
namespace json = boost::json;
namespace po = boost::program_options;

constexpr const char *_VER = "0.2.1";

/* http server operation */
void registerRoutes(HttpServer &server);
http::response<http::string_body> handleProxy(
		const http::request<http::string_body>& req,
		const std::unordered_map<std::string, std::string>& params,
		const char *method);
http::response<http::string_body> handleLogin(
		const http::request<http::string_body>& req,
		const std::unordered_map<std::string, std::string>& params);

/**/
bool is_special_header(const std::string &header);


static std::string	_ADDR;
static int		_PORT;
static std::string	_DEFT_PATH;
static std::unique_ptr<MyRSA::Generator>	_KEYGEN;
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
			_DEFT_PATH = cf.get<std::string>("Proxy.default", "");
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

	OSSL_PROVIDER *legacy = OSSL_PROVIDER_load(NULL, "legacy");
	if (legacy == NULL) {
		unsigned long err = ERR_get_error();
		char err_buf[256];
		ERR_error_string_n(err, err_buf, sizeof(err_buf));
		std::cerr << "Failed to load legacy provider: " << err_buf << std::endl;
		// 可能原因：模块未找到、路径不对、版本不兼容等
	} else {
		std::cout << "Legacy provider loaded successfully." << std::endl;
		// 记住保存 legacy 指针，程序退出前调用 OSSL_PROVIDER_unload(legacy);
	}

	_KEYGEN = std::make_unique<MyRSA::Generator> ();

	server.start();
	server.run();

	OSSL_PROVIDER_unload(legacy);
}

void registerRoutes(HttpServer &server) {

	server.post("/login", [](const auto& req, const auto& params) {
		return handleLogin(req, params);
	});

	server.get("*", [](const auto& req, const auto& params) {
		return handleProxy(req, params, "get");
	});

	server.post("*", [](const auto& req, const auto& params) {
		return handleProxy(req, params, "post");
	});
}

/* curl -X POST http://127.0.0.1:9201/login  */
http::response<http::string_body> handleLogin(
	const http::request<http::string_body>& req,
	const std::unordered_map<std::string, std::string>& params)
{
	http::response<http::string_body> res;
	res.version(req.version());

	try {
		auto _PRIV = MyRSA::Private_Key::from_pem(_KEYGEN->get_private_key_pem());
		auto _PUBL = MyRSA::Public_Key::from_pem(_KEYGEN->get_public_key_pem());

		std::string data;
		std::string hash;
		std::string sign;
		std::string token;

		data = "hello,world";
		hash = SHA256::sha256(data);
		sign = _PRIV.Sign(hash);

		json::value jv = {{"user", data}, {"hash", hash}, {"sign", sign}};
		token = Base64::base64_encode(json::serialize(jv));

		res.result(http::status::ok);
		res.body() = token;
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

/* curl -H "Target-Url: https://www.baidu.com" http://127.0.0.1:9201 -X GET */
http::response<http::string_body> handleProxy(
	const http::request<http::string_body>& req,
	const std::unordered_map<std::string, std::string>& params,
	const char *method)
{
	http::response<http::string_body> res;
	res.version(req.version());

	try {

		std::string url = "";
		auto flag = false;
		{
			auto it = req.base().find("Target-Url");
			if (it != req.base().end()) {
				url = req["Target-Url"];
			}

			it = req.base().find("target-url");
			if (it != req.base().end()) {
				url = req["target-url"];
			}

			if (url == "") {
				url =  _DEFT_PATH;
				flag = true;
			}

			url += "/";
			url += req.target();
		}

		/* check auth */
		std::string auth = "";
		{
			auto it = req.base().find("Authorization");
			if (it != req.base().end()) {
				std::string raw = req["Authorization"];
				if (raw.substr(0, 7) == "Bearer ") {
					auth = Base64::base64_decode_to_string(raw.substr(7, raw.size()-7));
					std::cout << "auth data: " << auth  << std::endl;
				}
			}
		}

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
			client.set_header(h);
		}


		if (method == "get")
			client.get_stream_sync(url, callback);
	
		else if (method == "post") {
			std::string payload = req.body();
			client.post_stream_sync(url, payload, callback);
		}


		for (const auto& pair: client.get_response_headers() ) {
			res.set(pair.first, pair.second);
		}
		res.result(client.get_response_code());
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

	return  header == "Host" || header == "connection"
		|| header == "Accept-Encoding";
}
