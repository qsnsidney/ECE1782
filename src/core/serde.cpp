#include "serde.h"
#include "utst.h"

#include <regex>
#include <fstream>
#include <iostream>

namespace
{
    template <typename T = CORE::XYZ::value_type>
    T str_to_floating(const std::string &str)
    {
        if constexpr (std::is_same_v<T, float>)
        {
            return std::stof(str);
        }
        else
        {
            return std::stod(str);
        }
    }

    template <typename T>
    void write_as_binary(std::ostream &os, T value)
    {
        os.write(reinterpret_cast<const char *>(&value), sizeof(T));
    }

    template <typename T>
    T read_as_binary(std::istream &is)
    {
        T value;
        is.read(reinterpret_cast<char *>(&value), sizeof(T));
        return value;
    }
}

namespace CORE
{
    std::vector<BODY_IC> parse_body_ic_from_csv(std::istream &csv_istream)
    {
        std::vector<BODY_IC> body_ics;
        const std::regex row_regex("(.*),(.*),(.*),(.*),(.*),(.*),(.*),?", std::regex::optimize);

        std::string row_str;

        while (std::getline(csv_istream, row_str))
        {
            std::smatch m;
            std::regex_match(row_str, m, row_regex);
            if (m.size() != 8)
            {
                // Invalid csv
                std::cout << "Invalid CSV: " << m.size() << " matches for row:" << std::endl;
                std::cout << row_str << std::endl;
                return {};
            }

            POS p{str_to_floating(m[1]), str_to_floating(m[2]), str_to_floating(m[3])};
            VEL v{str_to_floating(m[4]), str_to_floating(m[5]), str_to_floating(m[6])};
            MASS mass{str_to_floating(m[7])};

            body_ics.emplace_back(p, v, mass);
        }
        return body_ics;
    }

    std::vector<BODY_IC> parse_body_ic_from_csv(const std::string &csv_file_path)
    {
        std::ifstream csv_file_ifstream(csv_file_path);
        UTST_ASSERT(csv_file_ifstream.is_open());
        return parse_body_ic_from_csv(csv_file_ifstream);
    }

    void serialize_body_ic_to_bin(std::ostream &bin_ostream, const std::vector<BODY_IC> &body_ics)
    {
        // - first 4 bytes: size of floating type (ie., 4 for floating, 8 for double)
        const int size_floating_value_type = sizeof(UNIVERSE::floating_value_type);
        write_as_binary(bin_ostream, size_floating_value_type);

        /// - second 4 bytes: number of bodies
        const int num_bodies = body_ics.size();
        write_as_binary(bin_ostream, num_bodies);

        // - rest: (POS.x,POS.y,POS.z,VEL.x,VEL.y,VEL.z, MASS) for each BODY_IC
        for (const auto &body_ic : body_ics)
        {
            const auto &body_pos = std::get<POS>(body_ic);
            const auto &body_vel = std::get<VEL>(body_ic);
            const auto body_mass = std::get<MASS>(body_ic);

            write_as_binary(bin_ostream, body_pos.x);
            write_as_binary(bin_ostream, body_pos.y);
            write_as_binary(bin_ostream, body_pos.z);
            write_as_binary(bin_ostream, body_vel.x);
            write_as_binary(bin_ostream, body_vel.y);
            write_as_binary(bin_ostream, body_vel.z);
            write_as_binary(bin_ostream, body_mass);
        }
    }

    void serialize_body_ic_to_bin(const std::string &bin_file_path, const std::vector<BODY_IC> &body_ics)
    {
        std::ofstream dest_file_ofstream(bin_file_path, std::ios::binary);
        UTST_ASSERT(dest_file_ofstream.is_open());
        serialize_body_ic_to_bin(dest_file_ofstream, body_ics);
    }

    std::vector<BODY_IC> deserialize_body_ic_from_bin(std::istream &bin_istream)
    {
        std::vector<BODY_IC> body_ics;

        // - first 4 bytes: size of floating type (ie., 4 for floating, 8 for double)
        // Note: if os is not in binary mode, then size_floating_value_type might not
        // be displayable
        const int expected_size_floating_value_type = sizeof(UNIVERSE::floating_value_type);
        const auto size_floating_value_type = read_as_binary<int>(bin_istream);
        UTST_ASSERT_EQUAL(expected_size_floating_value_type, size_floating_value_type);

        /// - second 4 bytes: number of bodies
        const auto num_bodies = read_as_binary<int>(bin_istream);
        body_ics.reserve(num_bodies);

        // - rest: (POS.x,POS.y,POS.z,VEL.x,VEL.y,VEL.z, MASS) for each BODY_IC
        for (int count_bodies = 0; count_bodies < num_bodies; count_bodies++)
        {
            POS body_pos;
            VEL body_vel;
            MASS body_mass;

            body_pos.x = read_as_binary<UNIVERSE::floating_value_type>(bin_istream);
            body_pos.y = read_as_binary<UNIVERSE::floating_value_type>(bin_istream);
            body_pos.z = read_as_binary<UNIVERSE::floating_value_type>(bin_istream);
            body_vel.x = read_as_binary<UNIVERSE::floating_value_type>(bin_istream);
            body_vel.y = read_as_binary<UNIVERSE::floating_value_type>(bin_istream);
            body_vel.z = read_as_binary<UNIVERSE::floating_value_type>(bin_istream);
            body_mass = read_as_binary<UNIVERSE::floating_value_type>(bin_istream);

            body_ics.emplace_back(body_pos, body_vel, body_mass);
        }

        return body_ics;
    }

    std::vector<BODY_IC> deserialize_body_ic_from_bin(const std::string &bin_file_path)
    {
        std::ifstream bin_file_ifstream(bin_file_path, std::ios::binary);
        UTST_ASSERT(bin_file_ifstream.is_open());
        return deserialize_body_ic_from_bin(bin_file_ifstream);
    }
}