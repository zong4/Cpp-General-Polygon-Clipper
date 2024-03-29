﻿#pragma once

#include <vector>
#include "geometry/gpc_aet.hpp"
#include "geometry/gpc_lmt.hpp"
#include "geometry/gpc_polygon.hpp"
#include "geometry/gpc_tristrip.hpp"
#include "utilis/gpc_math.hpp"

namespace gpc {

void gpc_polygon_clip(gpc_op set_operation, gpc_polygon &subject_polygon,
                      gpc_polygon &clip_polygon, gpc_polygon &result_polygon);

void gpc_polygon_to_tristrip(gpc_polygon *polygon, gpc_tristrip *tristrip);

} // namespace gpc
