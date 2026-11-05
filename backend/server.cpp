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
#include "alu_report.hpp"
#include "compare.hpp"
#include "four_stage_session.hpp"
#include "json_util.hpp"
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

const std::string BENCHMARKS_FOLDER = "benchmarks"; // inside programs/

bool is_asm_file(const std::filesystem::directory_entry& entry) {
    std::string name = entry.path().filename().string();
    return entry.is_regular_file() && name.size() > 4 && name.substr(name.size() - 4) == ".asm";
}

// The .asm files under programs/, as paths relative to it (e.g. "benchmarks/alg01_bubble_sort.asm"), sorted
std::vector<std::string> list_programs() {
    std::vector<std::string> names;
    if (!std::filesystem::is_directory(PROGRAMS_FOLDER)) {
        return names;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(PROGRAMS_FOLDER)) {
        if (is_asm_file(entry)) {
            names.push_back(std::filesystem::relative(entry.path(), PROGRAMS_FOLDER).generic_string());
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

// A program name sent by the browser must be one of the listed files (nothing outside programs/)
bool is_listed_program(const std::string& name) {
    std::vector<std::string> names = list_programs();
    return std::find(names.begin(), names.end(), name) != names.end();
}

// A benchmark's header comments: "@ title", "@ what it tests...", "@ category: x", "@ expect: ..."
json benchmark_info(const std::string& name, const std::string& source) {
    json j;
    j["name"] = name;
    j["title"] = "";
    j["about"] = "";
    j["category"] = "";
    j["expect"] = "";
    std::istringstream lines(source);
    std::string line;
    while (std::getline(lines, line) && line.rfind("@ ", 0) == 0) {
        std::string text = line.substr(2);
        if (text.rfind("category: ", 0) == 0) {
            j["category"] = text.substr(10);
        } else if (text.rfind("expect: ", 0) == 0) {
            j["expect"] = text.substr(8);
        } else if (j["title"] == "") {
            j["title"] = text;
        } else {
            std::string about = j["about"].get<std::string>();
            j["about"] = about.empty() ? text : about + " " + text;
        }
    }
    return j;
}

// ?processor=four picks the four-stage processor; without it the six-stage one answers
bool wants_four(const httplib::Request& req) {
    return req.get_param_value("processor") == "four";
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

    std::unique_ptr<web::Session> session; // the six-stage processor
    std::unique_ptr<web::FourStageSession> four_session;
    std::unique_ptr<web::Comparer> comparer; // fresh runs of all three processors, for the Compare page
    try {
        session = std::make_unique<web::Session>(microcode_path);
        four_session = std::make_unique<web::FourStageSession>(microcode_path);
        comparer = std::make_unique<web::Comparer>(microcode_path);
        if (std::filesystem::exists(program_path)) {
            std::string name = std::filesystem::path(program_path).filename().string();
            std::string source = read_file(program_path);
            session->load(name, source);
            four_session->load(name, source);
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    std::mutex lock; // the server answers requests on several threads; one session per processor
    httplib::Server server;

    server.Get("/api/program", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> guard(lock);
        if (wants_four(req)) {
            send_json(res, four_session->program_json());
        } else {
            send_json(res, session->program_json());
        }
    });

    server.Get("/api/state", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> guard(lock);
        if (wants_four(req)) {
            send_json(res, four_session->state_json());
        } else {
            send_json(res, session->state_json());
        }
    });

    // The .asm files under programs/
    server.Get("/api/programs", [&](const httplib::Request&, httplib::Response& res) {
        send_json(res, list_programs());
    });

    // The benchmark programs in programs/benchmarks/ with their header comments
    server.Get("/api/benchmarks", [&](const httplib::Request&, httplib::Response& res) {
        json list = json::array();
        for (const std::string& name : list_programs()) {
            if (name.rfind(BENCHMARKS_FOLDER + "/", 0) == 0) {
                list.push_back(benchmark_info(name, read_file(PROGRAMS_FOLDER + "/" + name)));
            }
        }
        send_json(res, list);
    });

    // Body: {"program": "benchmarks/alg01_bubble_sort.asm"} or {"source": "..."}.
    // Runs it on fresh 4-stage, 6-stage and sequential processors, with the pipelines' timelines.
    server.Post("/api/compare", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            std::string source;
            json info;
            if (body.contains("program")) {
                std::string name = body["program"].get<std::string>();
                if (!is_listed_program(name)) {
                    send_error(res, 400, "not a program in " + PROGRAMS_FOLDER + "/: " + name);
                    return;
                }
                source = read_file(PROGRAMS_FOLDER + "/" + name);
                info = benchmark_info(name, source);
            } else {
                source = body.at("source").get<std::string>();
                info = benchmark_info("untitled.asm", source);
            }
            json result = comparer->compare(source, true);
            result["program"] = info;
            send_json(res, result);
        } catch (const std::exception& error) {
            send_error(res, 400, error.what());
        }
    });

    // Every benchmark on the three processors (no timelines)
    server.Get("/api/benchmark", [&](const httplib::Request&, httplib::Response& res) {
        try {
            json results = json::array();
            for (const std::string& name : list_programs()) {
                if (name.rfind(BENCHMARKS_FOLDER + "/", 0) != 0) {
                    continue;
                }
                std::string source = read_file(PROGRAMS_FOLDER + "/" + name);
                json result = comparer->compare(source, false);
                result["program"] = benchmark_info(name, source);
                results.push_back(result);
            }
            send_json(res, results);
        } catch (const std::exception& error) {
            send_error(res, 500, error.what());
        }
    });

    // Body: {"program": "factorial.asm"} to load a file from programs/,
    // or {"name": "...", "source": "..."} to load assembly text. Both processors get the program.
    server.Post("/api/load", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> guard(lock);
        try {
            json body = json::parse(req.body);
            if (body.contains("program")) {
                std::string name = body["program"].get<std::string>();
                if (!is_listed_program(name)) {
                    send_error(res, 400, "not a program in " + PROGRAMS_FOLDER + "/: " + name);
                    return;
                }
                std::string source = read_file(PROGRAMS_FOLDER + "/" + name);
                session->load(name, source);
                four_session->load(name, source);
            } else {
                std::string name = "untitled.asm";
                if (body.contains("name")) {
                    name = body["name"].get<std::string>();
                }
                std::string source = body.at("source").get<std::string>();
                session->load(name, source);
                four_session->load(name, source);
            }
            if (wants_four(req)) {
                send_json(res, four_session->program_json());
            } else {
                send_json(res, session->program_json());
            }
        } catch (const std::exception& error) {
            send_error(res, 400, error.what());
        }
    });

    server.Post("/api/reset", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> guard(lock);
        if (wants_four(req)) {
            four_session->reset();
            send_json(res, four_session->state_json());
        } else {
            session->reset();
            send_json(res, session->state_json());
        }
    });

    // Body: {"adder": "prefix", "multiplier": "booth4", "divider": "nonrestoring"}; both processors
    // switch to it and start the program again
    server.Post("/api/alu-config", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> guard(lock);
        try {
            alu::AluConfig config = web::alu_config_from_json(json::parse(req.body));
            session->configure_alu(config);
            four_session->configure_alu(config);
            if (wants_four(req)) {
                send_json(res, four_session->state_json());
            } else {
                send_json(res, session->state_json());
            }
        } catch (const std::exception& error) {
            send_error(res, 400, error.what());
        }
    });

    // The ALU page: the units measured from their gates, and every benchmark run with each algorithm.
    // Takes several seconds the first time, so the answer is kept.
    std::mutex alu_lock;
    json alu_report;
    server.Get("/api/alu", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> guard(alu_lock);
        try {
            if (alu_report.is_null()) {
                json programs = json::array();
                std::vector<std::string> sources;
                for (const std::string& name : list_programs()) {
                    if (name.rfind(BENCHMARKS_FOLDER + "/", 0) != 0) {
                        continue;
                    }
                    std::string source = read_file(PROGRAMS_FOLDER + "/" + name);
                    programs.push_back(benchmark_info(name, source));
                    sources.push_back(source);
                }
                json report;
                report["units"] = web::alu_units_json();
                report["programs"] = programs;
                report["runs"] = web::alu_runs_json(*comparer, sources);
                alu_report = report;
            }
            send_json(res, alu_report);
        } catch (const std::exception& error) {
            send_error(res, 500, error.what());
        }
    });

    // Body: {"unit": "microcycle" or "beat", "count": n}
    server.Post("/api/step", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> guard(lock);
        try {
            json body = json::parse(req.body.empty() ? "{}" : req.body);
            std::string unit = body.value("unit", "microcycle");
            int count = body.value("count", 1);
            for (int i = 0; i < count; i++) {
                if (wants_four(req) && unit == "beat") {
                    four_session->step_beat();
                } else if (wants_four(req)) {
                    four_session->step_microcycle();
                } else if (unit == "beat") {
                    session->step_beat();
                } else {
                    session->step_microcycle();
                }
            }
            if (wants_four(req)) {
                send_json(res, four_session->state_json());
            } else {
                send_json(res, session->state_json());
            }
        } catch (const std::exception& error) {
            send_error(res, 400, error.what());
        }
    });

    // Run until the program halts or raises an exception
    server.Post("/api/run", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard<std::mutex> guard(lock);
        if (wants_four(req)) {
            four_session->run(MAX_RUN_MICROCYCLES);
            send_json(res, four_session->state_json());
        } else {
            session->run(MAX_RUN_MICROCYCLES);
            send_json(res, session->state_json());
        }
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
