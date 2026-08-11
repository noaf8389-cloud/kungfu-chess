// https://github.com/noaf8389-cloud/CTD_CHAYA_FLAM.git

#include "server.hpp"
#include "logging/logger.hpp"

int main(int argc, char** argv) {
    std::string layoutPath = argc > 1 ? argv[1] : "../../ui/CTD26-main/layout_standard.txt";
    Logger::init("server.log");
    Server server(layoutPath, 8080, 8081);
    return server.run();
}