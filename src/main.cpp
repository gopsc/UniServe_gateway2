#include <us/HttpServer.hpp>
#include <iostream>
void registerRoutes(pmc::net::HttpServer &server);
int main() {
	pmc::net::HttpServer server("127.0.0.1", 9201, 4);
	registerRoutes(server);
	std::cout << "helloworld" << std::endl;
}

void registerRoutes(pmc::net::HttpServer &server) {

	;
}
