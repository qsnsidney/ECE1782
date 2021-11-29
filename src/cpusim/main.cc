#include <iostream>
#include <memory>
#include <optional>

#include "core/macros.hpp"
#include "core/serde.h"
#include "core/engine.h"
#include "core/timer.h"
#include "core/cxxopts.hpp"
#include "simple_engine.h"
#include "mt_engine.h"

auto parse_args(int argc, const char *argv[])
{
    cxxopts::Options options(argv[0]);
    options
        .positional_help("[optional args]")
        .show_positional_help()
        .set_tab_expansion()
        .allow_unrecognised_options();

    auto option_group = options.add_options();
    option_group("i,ic_file", "ic_file: .bin or .csv", cxxopts::value<std::string>());
    option_group("b,num_bodies", "max_n_bodies: optional (default -1), no effect if < 0 or >= n_body from ic_file", cxxopts::value<int>()->default_value("-1"));
    option_group("d,dt", "dt", cxxopts::value<CORE::UNIVERSE::floating_value_type>());
    option_group("n,num_iterations", "num_iterations", cxxopts::value<int>());
    option_group("t,num_threads", "num_threads for CPU", cxxopts::value<int>()->default_value("1"));
    option_group("o,out", "body_states_log_dir: optional", cxxopts::value<std::string>());
    option_group("v,verbose", "verbosity: can stack, optional");
    option_group("h,help", "Print usage");

    auto result = options.parse(argc, argv);

    if (result.count("help"))
    {
        std::cout << options.help() << std::endl;
        exit(0);
    }

    return result;
}

int main(int argc, const char *argv[])
{
    CORE::TIMER timer("cpusim");

    // Load args
    auto arg_result = parse_args(argc, argv);
    const std::string ic_file_path = arg_result["ic_file"].as<std::string>();
    const int max_n_body = arg_result["num_bodies"].as<int>();
    const CORE::DT dt = arg_result["dt"].as<CORE::UNIVERSE::floating_value_type>();
    const int n_iteration = arg_result["num_iterations"].as<int>();
    const int n_thread = arg_result["num_threads"].as<int>();
    std::optional<std::string> body_states_log_dir_opt = {};
    if (arg_result.count("out"))
    {
        body_states_log_dir_opt = arg_result["out"].as<std::string>();
    }
    const int verbosity = arg_result.count("verbose");
    CORE::TIMER::set_trigger_level(static_cast<CORE::TIMER::TRIGGER_LEVEL>(verbosity));

    std::cout << "Running.." << std::endl;
    std::cout << "ic_file: " << ic_file_path << std::endl;
    std::cout << "max_n_body: " << max_n_body << std::endl;
    std::cout << "dt: " << dt << std::endl;
    std::cout << "n_iteration: " << n_iteration << std::endl;
    std::cout << "n_thread: " << n_thread << std::endl;
    std::cout << "body_states_log_dir: " << (body_states_log_dir_opt ? *body_states_log_dir_opt : std::string("null")) << std::endl;
    std::cout << "verbosity: " << verbosity << std::endl;
    std::cout << std::endl;
    timer.elapsed_previous("parsing_args");

    // Load ic
    CORE::BODY_STATE_VEC
        body_states = CORE::deserialize_body_state_vec_from_file(ic_file_path);
    if (max_n_body >= 0 && max_n_body < static_cast<int>(body_states.size()))
    {
        body_states.resize(max_n_body);
        std::cout << "Limiting number of bodies to " << max_n_body << std::endl;
    }
    timer.elapsed_previous("loading_ic");

    // Select engine here
    std::unique_ptr<CORE::ENGINE> engine;
    if (n_thread == 1)
    {
        engine.reset(new CPUSIM::SIMPLE_ENGINE(std::move(body_states), dt, body_states_log_dir_opt));
    }
    else
    {
        engine.reset(new CPUSIM::MT_ENGINE(std::move(body_states), dt, n_thread, body_states_log_dir_opt));
    }
    timer.elapsed_previous("initializing_engine");

    // Execute engine
    engine->run(n_iteration);
    timer.elapsed_previous("running_engine");

    return 0;
}