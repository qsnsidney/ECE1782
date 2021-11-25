#pragma once

#include <vector>
#include <optional>
#include "physics.h"

namespace CORE
{
    /// Interface
    class ENGINE
    {
    public:
        ENGINE(CORE::BODY_STATE_VEC body_states_ic, CORE::DT dt, std::optional<std::string> body_states_log_dir_opt = {});
        virtual ~ENGINE() = 0;

        // Main entrance
        virtual void run(int n_iter) final;

    protected:
        // To be defined
        // Continues execution from previous BODY_STATE_VEC
        virtual CORE::BODY_STATE_VEC execute(int n_iter) = 0;

    protected:
        const CORE::BODY_STATE_VEC &body_states_ic() const { return body_states_ic_; }
        CORE::DT dt() const { return dt_; }

        bool is_body_states_logging_enabled() const { return body_states_log_dir_opt_.has_value(); }
        // P signature: CORE::BODY_STATE_VEC body_states_producer()
        template <typename P>
        void push_body_states_to_log(P body_states_producer)
        {
            if (is_body_states_logging_enabled())
                push_body_states_to_log(body_states_producer());
        }
        void push_body_states_to_log(CORE::BODY_STATE_VEC body_states);
        void serialize_body_states_log();

    private:
        void set_body_states_ic(CORE::BODY_STATE_VEC body_states_ic);

    private:
        CORE::BODY_STATE_VEC body_states_ic_;
        CORE::DT dt_;

        std::optional<std::string> body_states_log_dir_opt_;
        std::vector<CORE::BODY_STATE_VEC> body_states_log_;
        int num_body_states_log_popped_ = 0;
    };
}