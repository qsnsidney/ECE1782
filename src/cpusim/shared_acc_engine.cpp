#include "shared_acc_engine.h"
#include "core/timer.h"
#include "core/utility.hpp"

#include <iostream>
#include "buffer.h"

namespace CPUSIM
{
    void SHARED_ACC_ENGINE::compute_acceleration(std::vector<CORE::ACC> &acc,
                                                 const std::vector<CORE::POS> &pos,
                                                 const std::vector<CORE::MASS> &mass)
    {
        const size_t n_body = mass.size();
        ASSERT(acc.size() == n_body);

        for (auto &a : acc)
        {
            a.reset();
        }

        if (n_thread() == 1)
        {
            for (size_t i_target_body = 0; i_target_body < n_body; i_target_body++)
            {
                for (size_t j_source_body = i_target_body + 1; j_source_body < n_body; j_source_body++)
                {
                    const CORE::ACC tgt_to_src{CORE::universal_field(pos[j_source_body], pos[i_target_body])};
                    acc[i_target_body] += mass[j_source_body] * tgt_to_src;
                    acc[j_source_body] -= mass[i_target_body] * tgt_to_src;
                }
            }
        }
        else
        {
            // parallel_for_helper(0, n_body, [&buf_in](int i_target_body)
            //                     { buf_in.acc[i_target_body].reset(); });

            const size_t num_pairs = n_body * (n_body - 1) / 2;
            for (size_t pair_id = 0; pair_id < num_pairs; pair_id++)
            {
                auto [i_target_body, j_source_body] = CORE::delinearize_upper_triangle_matrix_index(pair_id, n_body);
                const CORE::ACC tgt_to_src{CORE::universal_field(pos[j_source_body], pos[i_target_body])};
                acc[i_target_body] += mass[j_source_body] * tgt_to_src;
                acc[j_source_body] -= mass[i_target_body] * tgt_to_src;
            }
        }
    }

    CORE::BODY_STATE_VEC SHARED_ACC_ENGINE::execute(int n_iter)
    {
        const size_t n_body = body_states_ic().size();

        CORE::TIMER timer(std::string("SHARED_ACC_ENGINE(") + std::to_string(n_body) + "," + std::to_string(dt()) + "*" + std::to_string(n_iter) + ")");

        std::vector<CORE::MASS> mass(n_body, 0);
        BUFFER buf_in(n_body);
        // Step 1: Prepare ic
        for (size_t i_body = 0; i_body < n_body; i_body++)
        {
            const auto &[body_pos, body_vel, body_mass] = body_states_ic()[i_body];
            buf_in.pos[i_body] = body_pos;
            buf_in.vel[i_body] = body_vel;
            mass[i_body] = body_mass;
        }
        timer.elapsed_previous("step1");

        // Step 2: Prepare acceleration for ic
        compute_acceleration(buf_in.acc, buf_in.pos, mass);

        // Verify
        if (n_thread() != 1)
        {
            std::cout << "Verifying IC acceleration" << std::endl;
            std::vector<CORE::ACC> expected_acc(n_body);
            for (auto &acc : expected_acc)
            {
                acc.reset();
            }
            for (size_t i_target_body = 0; i_target_body < n_body; i_target_body++)
            {
                for (size_t j_source_body = i_target_body + 1; j_source_body < n_body; j_source_body++)
                {
                    const CORE::ACC tgt_to_src{CORE::universal_field(buf_in.pos[j_source_body], buf_in.pos[i_target_body])};
                    expected_acc[i_target_body] += mass[j_source_body] * tgt_to_src;
                    expected_acc[j_source_body] -= mass[i_target_body] * tgt_to_src;
                }
            }
            for (size_t i = 0; i < n_body; i++)
            {
                if (buf_in.acc[i] != expected_acc[i])
                {
                    std::cout << "buf_in.acc[" << i << "]: " << buf_in.acc[i] << std::endl;
                    std::cout << "expected_acc[" << i << "]: " << expected_acc[i] << std::endl;
                    ASSERT(false);
                }
            }
        }
        timer.elapsed_previous("step2");

        BUFFER buf_out(n_body);
        std::vector<CORE::VEL> vel_tmp(n_body);
        // Core iteration loop
        for (int i_iter = 0; i_iter < n_iter; i_iter++)
        {
            if (false)
            {
                debug_workspace(buf_in, mass);
            }

            for (size_t i_target_body = 0; i_target_body < n_body; i_target_body++)
            {
                // Step 3: Compute temp velocity
                vel_tmp[i_target_body] = CORE::VEL::updated(buf_in.vel[i_target_body], buf_in.acc[i_target_body], dt());

                // Step 4: Update position
                buf_out.pos[i_target_body] = CORE::POS::updated(buf_in.pos[i_target_body], buf_in.vel[i_target_body], buf_in.acc[i_target_body], dt());
            }

            // Step 5: Compute acceleration
            compute_acceleration(buf_out.acc, buf_out.pos, mass);

            // Step 6: Update velocity
            for (size_t i_target_body = 0; i_target_body < n_body; i_target_body++)
            {
                buf_out.vel[i_target_body] = CORE::VEL::updated(vel_tmp[i_target_body], buf_out.acc[i_target_body], dt());
            }

            // Write BODY_STATE_VEC to log
            if (i_iter == 0)
            {
                push_body_states_to_log([&]()
                                        { return generate_body_state_vec(buf_in, mass); });
            }
            push_body_states_to_log([&]()
                                    { return generate_body_state_vec(buf_out, mass); });
            if (i_iter % 10 == 0)
            {
                serialize_body_states_log();
            }

            // Prepare for next iteration
            std::swap(buf_in, buf_out);

            timer.elapsed_previous(std::string("iter") + std::to_string(i_iter), CORE::TIMER::TRIGGER_LEVEL::INFO);
        }

        timer.elapsed_previous("all_iters");

        return generate_body_state_vec(buf_in, mass);
    }
}