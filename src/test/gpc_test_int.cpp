#include <gtest/gtest.h>
#include <iostream>

#include "../gpc.hpp"
#include "../utilis/gpc_file_system.hpp"

TEST(gpc_polygon_clip, test_int_1)
{
    gpc::gpc_polygon subject_polygon;
    gpc::gpc_polygon clip_polygon;

    subject_polygon.add_contour(gpc::gpc_vertex_list(
        {{0, 0}, {100, 0}, {100, 100}, {125, 125}, {-25, 125}, {0, 100}}));

    clip_polygon.add_contour(
        gpc::gpc_vertex_list({{50, 50}, {150, 50}, {150, 150}, {50, 150}}));

    gpc::gpc_polygon result_polygon;
    gpc::gpc_polygon_clip(gpc::gpc_op::GPC_INT, subject_polygon, clip_polygon,
                          result_polygon);

    gpc::gpc_polygon expected_polygon;
    expected_polygon.add_contour(gpc::gpc_vertex_list(
        {{50, 50}, {100, 50}, {100, 100}, {125, 125}, {50, 125}}));

    EXPECT_TRUE(gpc::equal_sort(result_polygon, expected_polygon));
}