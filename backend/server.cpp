#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "httplib.h"
#include "nlohmann/json.hpp"
#include "session.hpp"

using namespace risc;
using nlohmann::json;

const std::string PROGRAMS_FOLDER = "programs";
const std::string FRONTEND_FOLDER = "frontend/dist";
const std::uint64_t MAX_RUN_MICROCYCLES = 1000000;

void send_json(httplib::Response& res, const json& body) {
    res.set_content(body.dump(), "application/json");
}

void send_error(httplib::Response& res, int status, const std::string& message) {
    json body;
    body["error"] = message;
    res.status = status;
    send_json(res, body);
}

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// A program name sent by the browser must be a plain file name inside programs/
bool is_plain_asm_name(const std::string& name) {
    if (name.empty() || name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
        return false;
    }
    if (name.find("..") != std::string::npos) {
        return false;
    }
    return name.size() > 4 && name.substr(name.size() - 4) == ".asm";
}

// Usage: risc_web [--port N] [--microcode file.masm] [--program file.asm]
// Run it from the project folder. The API is at http://localhost:N/api/...
int main(int argc, char* argv[]) {
    int port = 8080;
    std::string microcode_path = "microcode/vertical.masm";
    std::string program_path = PROGRAMS_FOLDER + "/factorial.asm";
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string option = argv[i];
        if (option == "--port") {
            port = std::atoi(argv[i + 1]);
        } else if (option == "--microcode") {
            microcode_path = argv[i + 1];
        } else if (option == "--program") {
            program_path = argv[i + 1];
        } else {
            std::cerr << "Usage: risc_web [--port N] [--microcode file.masm] [--program file.asm]\n";
            return 1;
        }
    }

    std::unique_ptr<web::Session> session;
    try {
        session = std::make_unique<web::Session>(microcode_path);
        if (std::filesystem::exists(program_path)) {
            std::string name = std::filesystem::path(program_path).filename().string();
            session->load(name, read_file(program_path));
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    std::mutex lock; // the server answers requests on several threads; one session
    httplib::Server server;

    server.Get("/api/program", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> guard(lock);
        send_json(res, session->program_json());
    });

    server.Get("/api/state", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> guard(lock);
        send_json(res, session->state_json());
    });

    // The .asm files in programs/
    server.Get("/api/programs", [&](const httplib::Request&, httplib::Response& res) {
        std::vector<std::string> names;
        if (std::filesystem::is_directory(PROGRAMS_FOLDER)) {
            for (const auto& entry : std::filesystem::directory_iterator(PROGRAMS_FOLDER)) {
                std::string name = entry.path().filename().string();
                if (entry.is_regular_file() && is_plain_asm_name(name)) {
                    names.push_back(name);
                }
            }
        }
        std::sort(names.begin(), names.end());
        send_json(res, names);
    });

    // Body: {"program": "factorial.asm"} to load a file from programs/,
    // or {"name": "...", "source": "..."} to load assembly text
    server.Post("/api/load", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> guard(lock);
        try {
            json body = json::parse(req.body);
            if (body.contains("program")) {
                std::string name = body["program"].get<std::string>();
                if (!is_plain_asm_name(name)) {
                    send_error(res, 400, "not a program in " + PROGRAMS_FOLDER + "/: " + name);
                    return;
                }
                session->load(name, read_file(PROGRAMS_FOLDER + "/" + name));
            } else {
                std::string name = "untitled.asm";
                if (body.contains("name")) {
                    name = body["name"].get<std::string>();
                }
                session->load(name, body.at("source").get<std::string>());
            }
            send_json(res, session->program_json());
        } catch (const std::exception& error) {
            send_error(res, 400, error.what());
        }
    });

    server.Post("/api/reset", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> guard(lock);
        session->reset();
        send_json(res, session->state_json());
    });

    // Body: {"unit": "microcycle" or "beat", "count": n}
    server.Post("/api/step", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> guard(lock);
        try {
            json body = json::parse(req.body.empty() ? "{}" : req.body);
            std::string unit = body.value("unit", "microcycle");
            int count = body.value("count", 1);
            for (int i = 0; i < count; i++) {
                if (unit == "beat") {
                    session->step_beat();
                } else {
                    session->step_microcycle();
                }
            }
            send_json(res, session->state_json());
        } catch (const std::exception& error) {
            send_error(res, 400, error.what());
        }
    });

    // Run until the program halts or raises an exception
    server.Post("/api/run", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> guard(lock);
        session->run(MAX_RUN_MICROCYCLES);
        send_json(res, session->state_json());
    });

    // The built frontend, if there is one (npm run build in frontend/)
    if (std::filesystem::is_directory(FRONTEND_FOLDER)) {
        server.set_mount_point("/", FRONTEND_FOLDER);
    }

    std::printf("RISC201 web backend on http://localhost:%d (API under /api)\n", port);
    if (!server.listen("127.0.0.1", port)) {
        std::cerr << "Error: cannot listen on port " << port << '\n';
        return 1;
    }
    return 0;
}
