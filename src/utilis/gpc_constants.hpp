#pragma once

#include <string>

#include "gpc_enum.hpp"

namespace gpc {

constexpr bool LEFT = 0;
constexpr bool RIGHT = 1;

constexpr bool ABOVE = 0;
constexpr bool BELOW = 1;

constexpr bool CLIP = 0;
constexpr bool SUBJ = 1;

#define INVERT_TRISTRIPS FALSE

const std::string GPC_VERSION = "2.33";
const std::string CPP_GPC_VERSION = "1.00";

// Horizontal edge state transitions within scanbeam boundary
const h_state next_h_state[3][6] = {
    // ABOVE BELOW CROSS
    // L R L R L R
    {h_state::BH, h_state::TH, h_state::TH, h_state::BH, h_state::NH,
     h_state::NH}, // h_state::NH
    {h_state::NH, h_state::NH, h_state::NH, h_state::NH, h_state::TH,
     h_state::TH}, //  h_state::BH
    {h_state::NH, h_state::NH, h_state::NH, h_state::NH, h_state::BH,
     h_state::BH}}; // h_state::TH

} // namespace gpc
