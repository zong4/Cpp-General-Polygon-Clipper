﻿#include "gpc.hpp"
#include <list>

#include "other.hpp"

namespace gpc {

void gpc_polygon_clip(gpc_op op, gpc_polygon &subj, gpc_polygon &clip,
                      gpc_polygon &result)
{
    // clear result
    result.contours.clear();

    if (subj.num_contours() == 0 && clip.num_contours() == 0)
    {
        return;
    }

    // TODO: 无自相交
    if (clip.num_contours() == 0)
    {
        if (op == gpc_op::GPC_INT)
        {
            return;
        }
        else
        {
            result = subj;
            return;
        }
    }

    if (subj.num_contours() == 0)
    {
        if ((op == gpc_op::GPC_INT) || (op == gpc_op::GPC_DIFF))
        {
            return;
        }
        else
        {
            result = clip;
            return;
        }
    }

    // subj 和 clip 都不为空
    // 确定可能有贡献的轮廓
    if ((op == gpc_op::GPC_INT) || (op == gpc_op::GPC_DIFF))
    {
        minimax_test(subj, clip, op);
    }

    // 构建局部最小表
    gpc_lmt lmt;
    lmt.build_lmt(subj, SUBJ, op);
    lmt.build_lmt(clip, CLIP, op);

    std::sort(lmt.sbtree.begin(), lmt.sbtree.end());
    const std::vector<double> &sbt = lmt.sbtree;

    // 如果没有轮廓有贡献，返回空结果
    if (lmt.lmt_list.empty())
    {
        return;
    }

    // 对于差集操作，反转剪切多边形
    int parity[2] = {LEFT, LEFT};
    if (op == gpc_op::GPC_DIFF) parity[CLIP] = RIGHT;

    polygon_node *cf = nullptr;
    std::vector<polygon_node *> out_poly;

    // 处理每个扫描线
    int scanbeam = 0;
    gpc_aet aet;
    while (scanbeam < sbt.size())
    {
        // 设置扫描线的底部和顶部
        double yb = sbt[scanbeam++];

        while (scanbeam < sbt.size())
        {
            if (sbt[scanbeam] == yb)
            {
                ++scanbeam;
            }
            else
            {
                break;
            }
        }

        double yt = sbt[scanbeam];
        double dy = yt - yb;

        // 扫描线边界处理
        // 将从这个局部最小值开始的边添加到 AET 中
        if (!lmt.lmt_list.empty())
        {
            if (lmt.lmt_list.front().first == yb)
            {
                // Add edges starting at this local minimum to the AET
                for (auto &&edge : lmt.lmt_list.front().second)
                {
                    aet.insert(edge);
                }

                lmt.lmt_list.pop_front(); // 移动到下一个局部最小值
            }
        }

        // 在 AET 中创建捆绑
        // 为第一条边设置捆绑字段
        aet.aet_list.front().bundle[ABOVE][aet.aet_list.front().type] =
            (aet.aet_list.front().top.y !=
             yb); // 如果边的顶部不在当前扫描线上，则设置为 TRUE
        aet.aet_list.front().bundle[ABOVE][!aet.aet_list.front().type] = false;
        aet.aet_list.front().bstate[ABOVE] = bundle_state::UNBUNDLED;

        for (auto it = std::next(aet.aet_list.begin());
             it != aet.aet_list.end(); ++it)
        {
            // Set up bundle fields of next edge
            it->bundle[ABOVE][it->type] = (it->top.y != yb);
            it->bundle[ABOVE][!it->type] = false;
            it->bstate[ABOVE] = bundle_state::UNBUNDLED;

            // 如果边在扫描线边界以上且与前一条边重合，则捆绑边
            if (it->bundle[ABOVE][it->type])
            {
                if (equal(std::prev(it)->xb(), it->xb()) &&
                    equal(std::prev(it)->dx, it->dx) &&
                    (std::prev(it)->top.y != yb))
                {
                    it->bundle[ABOVE][it->type] ^=
                        std::prev(it)->bundle[ABOVE][it->type];
                    it->bundle[ABOVE][!it->type] =
                        std::prev(it)->bundle[ABOVE][!it->type];
                    it->bstate[ABOVE] = bundle_state::BUNDLE_HEAD;

                    std::prev(it)->bundle[ABOVE][CLIP] = false;
                    std::prev(it)->bundle[ABOVE][SUBJ] = false;
                    std::prev(it)->bstate[ABOVE] = bundle_state::BUNDLE_TAIL;
                }
            }
        }

        // 设置虚拟的前一个 x 值
        double px = -DBL_MAX;

        int exists[2];

        // 设置多边形的水平状态为非水平
        h_state horiz[2] = {h_state::NH, h_state::NH};

        // 在此扫描线边界处理每条边
        for (auto &&edge : aet.aet_list)
        {
            exists[CLIP] =
                edge.bundle[ABOVE][CLIP] + (edge.bundle[BELOW][CLIP] << 1);
            exists[SUBJ] =
                edge.bundle[ABOVE][SUBJ] + (edge.bundle[BELOW][SUBJ] << 1);

            if (exists[CLIP] || exists[SUBJ])
            {
                // 设置边的边界侧
                edge.bside[CLIP] = parity[CLIP];
                edge.bside[SUBJ] = parity[SUBJ];

                // Determine contributing status and quadrant occupancies
                bool contributing = 0;
                bool bl = 0, br = 0, tl = 0, tr = 0;
                switch (op)
                {
                    case gpc_op::GPC_DIFF:
                    case gpc_op::GPC_INT:
                        contributing =
                            (exists[CLIP] && (parity[SUBJ] || horiz[SUBJ])) ||
                            (exists[SUBJ] && (parity[CLIP] || horiz[CLIP])) ||
                            (exists[CLIP] && exists[SUBJ] &&
                             (parity[CLIP] == parity[SUBJ]));

                        br = (parity[CLIP]) && (parity[SUBJ]);

                        bl = (parity[CLIP] ^ edge.bundle[ABOVE][CLIP]) &&
                             (parity[SUBJ] ^ edge.bundle[ABOVE][SUBJ]);

                        tr = (parity[CLIP] ^ (horiz[CLIP] != h_state::NH)) &&
                             (parity[SUBJ] ^ (horiz[SUBJ] != h_state::NH));

                        tl = (parity[CLIP] ^ (horiz[CLIP] != h_state::NH) ^
                              edge.bundle[BELOW][CLIP]) &&
                             (parity[SUBJ] ^ (horiz[SUBJ] != h_state::NH) ^
                              edge.bundle[BELOW][SUBJ]);
                        break;
                    case gpc_op::GPC_XOR:
                        contributing = exists[CLIP] || exists[SUBJ];

                        br = (parity[CLIP]) ^ (parity[SUBJ]);

                        bl = (parity[CLIP] ^ edge.bundle[ABOVE][CLIP]) ^
                             (parity[SUBJ] ^ edge.bundle[ABOVE][SUBJ]);

                        tr = (parity[CLIP] ^ (horiz[CLIP] != h_state::NH)) ^
                             (parity[SUBJ] ^ (horiz[SUBJ] != h_state::NH));

                        tl = (parity[CLIP] ^ (horiz[CLIP] != h_state::NH) ^
                              edge.bundle[BELOW][CLIP]) ^
                             (parity[SUBJ] ^ (horiz[SUBJ] != h_state::NH) ^
                              edge.bundle[BELOW][SUBJ]);
                        break;
                    case gpc_op::GPC_UNION:
                        contributing =
                            (exists[CLIP] && (!parity[SUBJ] || horiz[SUBJ])) ||
                            (exists[SUBJ] && (!parity[CLIP] || horiz[CLIP])) ||
                            (exists[CLIP] && exists[SUBJ] &&
                             (parity[CLIP] == parity[SUBJ]));

                        br = (parity[CLIP]) || (parity[SUBJ]);

                        bl = (parity[CLIP] ^ edge.bundle[ABOVE][CLIP]) ||
                             (parity[SUBJ] ^ edge.bundle[ABOVE][SUBJ]);

                        tr = (parity[CLIP] ^ (horiz[CLIP] != h_state::NH)) ||
                             (parity[SUBJ] ^ (horiz[SUBJ] != h_state::NH));

                        tl = (parity[CLIP] ^ (horiz[CLIP] != h_state::NH) ^
                              edge.bundle[BELOW][CLIP]) ||
                             (parity[SUBJ] ^ (horiz[SUBJ] != h_state::NH) ^
                              edge.bundle[BELOW][SUBJ]);
                        break;
                }

                // 更新奇偶性
                parity[CLIP] ^= edge.bundle[ABOVE][CLIP];
                parity[SUBJ] ^= edge.bundle[ABOVE][SUBJ];

                // 更新水平状态
                if (exists[CLIP])
                    horiz[CLIP] =
                        next_h_state[horiz[CLIP]]
                                    [((exists[CLIP] - 1) << 1) + parity[CLIP]];
                if (exists[SUBJ])
                    horiz[SUBJ] =
                        next_h_state[horiz[SUBJ]]
                                    [((exists[SUBJ] - 1) << 1) + parity[SUBJ]];

                vertex_type vclass = static_cast<vertex_type>(
                    tr + (tl << 1) + (br << 2) + (bl << 3));

                if (contributing)
                {
                    double xb = edge.xb();

                    switch (vclass)
                    {
                        case vertex_type::EMN: // 外部最小值
                        case vertex_type::IMN: // 内部最小值
                            add_local_min(out_poly, &edge, xb, yb);
                            px = xb;
                            cf = edge.outp[ABOVE];
                            break;
                        case vertex_type::ERI: // 外部右中间值
                            if (xb != px)
                            {
                                cf->add_right({xb, yb});
                                px = xb;
                            }

                            edge.outp[ABOVE] = cf;
                            cf = nullptr;
                            break;
                        case vertex_type::ELI: // 外部左中间值
                            edge.outp[BELOW]->add_left({xb, yb});
                            px = xb;
                            cf = edge.outp[BELOW];
                            break;
                        case vertex_type::EMX: // 外部最大值
                            if (xb != px)
                            {
                                cf->add_left({xb, yb});
                                px = xb;
                            }

                            merge_right(cf, edge.outp[BELOW], out_poly);
                            cf = nullptr;
                            break;
                        case vertex_type::ILI: // 内部左中间值
                            if (xb != px)
                            {
                                cf->add_left({xb, yb});
                                px = xb;
                            }

                            edge.outp[ABOVE] = cf;
                            cf = nullptr;
                            break;
                        case vertex_type::IRI: // 内部右中间值
                            edge.outp[BELOW]->add_right({xb, yb});
                            px = xb;
                            cf = edge.outp[BELOW];
                            edge.outp[BELOW] = nullptr;
                            break;
                        case vertex_type::IMX: // 内部最大值
                            if (xb != px)
                            {
                                cf->add_right({xb, yb});
                                px = xb;
                            }

                            merge_left(cf, edge.outp[BELOW], out_poly);
                            cf = nullptr;
                            edge.outp[BELOW] = nullptr;
                            break;
                        case vertex_type::IMM: // 内部最大值和最小值
                            if (xb != px)
                            {
                                cf->add_right({xb, yb});
                                px = xb;
                            }

                            merge_left(cf, edge.outp[BELOW], out_poly);
                            edge.outp[BELOW] = nullptr;
                            add_local_min(out_poly, &edge, xb, yb);
                            cf = edge.outp[ABOVE];
                            break;
                        case vertex_type::EMM: // 外部最大值和最小值
                            if (xb != px)
                            {
                                cf->add_left({xb, yb});
                                px = xb;
                            }

                            merge_right(cf, edge.outp[BELOW], out_poly);
                            edge.outp[BELOW] = nullptr;
                            add_local_min(out_poly, &edge, xb, yb);
                            cf = edge.outp[ABOVE];
                            break;
                        case vertex_type::LED: // 左边界
                            if (edge.bot.y == yb)
                            {
                                edge.outp[BELOW]->add_left({xb, yb});
                            }

                            edge.outp[ABOVE] = edge.outp[BELOW];
                            px = xb;
                            break;
                        case vertex_type::RED: // 右边界
                            if (edge.bot.y == yb)
                            {
                                edge.outp[BELOW]->add_right({xb, yb});
                            }

                            edge.outp[ABOVE] = edge.outp[BELOW];
                            px = xb;
                            break;
                        default: break;
                    } // End of switch
                } // End of contributing conditional
            } // End of edge exists conditional
        } // End of AET loop

        // Delete terminating edges from the AET, otherwise compute xt()
        for (auto it = aet.aet_list.begin(); it != aet.aet_list.end();)
        {
            if (it->top.y == yb)
            {
                // Copy bundle head state to the adjacent tail edge if
                // required
                if ((it != aet.aet_list.begin() &&
                     it->bstate[BELOW] == bundle_state::BUNDLE_HEAD) &&
                    std::prev(it)->bstate[BELOW] == bundle_state::BUNDLE_TAIL)
                {
                    std::prev(it)->outp[BELOW] = it->outp[BELOW];
                    std::prev(it)->bstate[BELOW] = bundle_state::UNBUNDLED;

                    if (std::prev(it) != aet.aet_list.begin() &&
                        std::prev(std::prev(it))->bstate[BELOW] ==
                            bundle_state::BUNDLE_TAIL)
                    {
                        std::prev(it)->bstate[BELOW] =
                            bundle_state::BUNDLE_HEAD;
                    }
                }

                it = aet.aet_list.erase(it);
            }
            else
            {
                // 更新 xtop
                if (it->top.y == yt)
                {
                    it->xt(it->top.x);
                }
                else
                {
                    it->xt(it->bot.x + it->dx * (yt - it->bot.y));
                }

                ++it;
            }
        }

        if (scanbeam < sbt.size())
        {
            // SCANBEAM INTERIOR PROCESSING
            std::list<it_node> it;
            build_intersection_table(it, aet.aet_list, dy);

            // Process each node in the intersection table
            for (auto &&intersect : it)
            {
                gpc_edge_node *e0 = intersect.ie[0];
                gpc_edge_node *e1 = intersect.ie[1];

                // Only generate output for contributing intersections
                if ((intersect.ie[0]->bundle[ABOVE][CLIP] ||
                     e0->bundle[ABOVE][SUBJ]) &&
                    (intersect.ie[1]->bundle[ABOVE][CLIP] ||
                     intersect.ie[1]->bundle[ABOVE][SUBJ]))
                {
                    polygon_node *p = e0->outp[ABOVE];
                    polygon_node *q = e1->outp[ABOVE];

                    double ix = intersect.point.x;
                    double iy = intersect.point.y + yb;

                    int in[2];
                    in[CLIP] =
                        (e0->bundle[ABOVE][CLIP] && !e0->bside[CLIP]) ||
                        (e1->bundle[ABOVE][CLIP] && e1->bside[CLIP]) ||
                        (!e0->bundle[ABOVE][CLIP] && !e1->bundle[ABOVE][CLIP] &&
                         e0->bside[CLIP] && e1->bside[CLIP]);
                    in[SUBJ] =
                        (e0->bundle[ABOVE][SUBJ] && !e0->bside[SUBJ]) ||
                        (e1->bundle[ABOVE][SUBJ] && e1->bside[SUBJ]) ||
                        (!e0->bundle[ABOVE][SUBJ] && !e1->bundle[ABOVE][SUBJ] &&
                         e0->bside[SUBJ] && e1->bside[SUBJ]);

                    // Determine quadrant occupancies
                    int bl = 0, br = 0, tl = 0, tr = 0;
                    switch (op)
                    {
                        case gpc_op::GPC_DIFF:
                        case gpc_op::GPC_INT:
                            tr = (in[CLIP]) && (in[SUBJ]);

                            tl = (in[CLIP] ^ e1->bundle[ABOVE][CLIP]) &&
                                 (in[SUBJ] ^ e1->bundle[ABOVE][SUBJ]);

                            br = (in[CLIP] ^ e0->bundle[ABOVE][CLIP]) &&
                                 (in[SUBJ] ^ e0->bundle[ABOVE][SUBJ]);

                            bl = (in[CLIP] ^ e1->bundle[ABOVE][CLIP] ^
                                  e0->bundle[ABOVE][CLIP]) &&
                                 (in[SUBJ] ^ e1->bundle[ABOVE][SUBJ] ^
                                  e0->bundle[ABOVE][SUBJ]);
                            break;
                        case gpc_op::GPC_XOR:
                            tr = (in[CLIP]) ^ (in[SUBJ]);

                            tl = (in[CLIP] ^ e1->bundle[ABOVE][CLIP]) ^
                                 (in[SUBJ] ^ e1->bundle[ABOVE][SUBJ]);

                            br = (in[CLIP] ^ e0->bundle[ABOVE][CLIP]) ^
                                 (in[SUBJ] ^ e0->bundle[ABOVE][SUBJ]);

                            bl = (in[CLIP] ^ e1->bundle[ABOVE][CLIP] ^
                                  e0->bundle[ABOVE][CLIP]) ^
                                 (in[SUBJ] ^ e1->bundle[ABOVE][SUBJ] ^
                                  e0->bundle[ABOVE][SUBJ]);
                            break;
                        case gpc_op::GPC_UNION:
                            tr = (in[CLIP]) || (in[SUBJ]);

                            tl = (in[CLIP] ^ e1->bundle[ABOVE][CLIP]) ||
                                 (in[SUBJ] ^ e1->bundle[ABOVE][SUBJ]);

                            br = (in[CLIP] ^ e0->bundle[ABOVE][CLIP]) ||
                                 (in[SUBJ] ^ e0->bundle[ABOVE][SUBJ]);

                            bl = (in[CLIP] ^ e1->bundle[ABOVE][CLIP] ^
                                  e0->bundle[ABOVE][CLIP]) ||
                                 (in[SUBJ] ^ e1->bundle[ABOVE][SUBJ] ^
                                  e0->bundle[ABOVE][SUBJ]);
                            break;
                    }

                    vertex_type vclass = static_cast<vertex_type>(
                        tr + (tl << 1) + (br << 2) + (bl << 3));

                    switch (vclass)
                    {
                        case vertex_type::EMN:
                            add_local_min(out_poly, e0, ix, iy);
                            e1->outp[ABOVE] = e0->outp[ABOVE];
                            break;
                        case vertex_type::ERI:
                            if (p)
                            {
                                p->add_right({ix, iy});
                                e1->outp[ABOVE] = p;
                                e0->outp[ABOVE] = nullptr;
                            }
                            break;
                        case vertex_type::ELI:
                            if (q)
                            {
                                q->add_left({ix, iy});
                                e0->outp[ABOVE] = q;
                                e1->outp[ABOVE] = nullptr;
                            }
                            break;
                        case vertex_type::EMX:
                            if (p && q)
                            {
                                p->add_left({ix, iy});
                                merge_right(p, q, out_poly);
                                e0->outp[ABOVE] = nullptr;
                                e1->outp[ABOVE] = nullptr;
                            }
                            break;
                        case vertex_type::IMN:
                            add_local_min(out_poly, e0, ix, iy);
                            e1->outp[ABOVE] = e0->outp[ABOVE];
                            break;
                        case vertex_type::ILI:
                            if (p)
                            {
                                p->add_left({ix, iy});
                                e1->outp[ABOVE] = p;
                                e0->outp[ABOVE] = nullptr;
                            }
                            break;
                        case vertex_type::IRI:
                            if (q)
                            {
                                q->add_right({ix, iy});
                                e0->outp[ABOVE] = q;
                                e1->outp[ABOVE] = nullptr;
                            }
                            break;
                        case vertex_type::IMX:
                            if (p && q)
                            {
                                p->add_right({ix, iy});
                                merge_left(p, q, out_poly);
                                e0->outp[ABOVE] = nullptr;
                                e1->outp[ABOVE] = nullptr;
                            }
                            break;
                        case vertex_type::IMM:
                            if (p && q)
                            {
                                p->add_right({ix, iy});
                                merge_left(p, q, out_poly);
                                add_local_min(out_poly, e0, ix, iy);
                                e1->outp[ABOVE] = e0->outp[ABOVE];
                            }
                            break;
                        case vertex_type::EMM:
                            if (p && q)
                            {
                                p->add_left({ix, iy});
                                merge_right(p, q, out_poly);
                                add_local_min(out_poly, e0, ix, iy);
                                e1->outp[ABOVE] = e0->outp[ABOVE];
                            }
                            break;
                        default: break;
                    } // End of switch
                } // End of contributing intersection conditional

                // Swap bundle sides in response to edge crossing
                if (e0->bundle[ABOVE][CLIP])
                {
                    e1->bside[CLIP] = !e1->bside[CLIP];
                }

                if (e1->bundle[ABOVE][CLIP])
                {
                    e0->bside[CLIP] = !e0->bside[CLIP];
                }

                if (e0->bundle[ABOVE][SUBJ])
                {
                    e1->bside[SUBJ] = !e1->bside[SUBJ];
                }

                if (e1->bundle[ABOVE][SUBJ])
                {
                    e0->bside[SUBJ] = !e0->bside[SUBJ];
                }

                // Swap the edge bundles in the aet.aet_list.
                swap_intersecting_edge_bundles(aet.aet_list, &intersect);

            } // End of IT loop

            // Prepare for next scanbeam
            for (auto it = aet.aet_list.begin(); it != aet.aet_list.end();)
            {
                gpc_edge_node *succ_edge = it->succ;
                if (it->top.y == yt && succ_edge)
                {
                    // Replace AET edge by its successor
                    succ_edge->outp[BELOW] = it->outp[ABOVE];

                    succ_edge->bstate[BELOW] = it->bstate[ABOVE];

                    succ_edge->bundle[BELOW][CLIP] = it->bundle[ABOVE][CLIP];
                    succ_edge->bundle[BELOW][SUBJ] = it->bundle[ABOVE][SUBJ];

                    aet.aet_list.insert(it, *succ_edge);
                    it = aet.aet_list.erase(it);
                }
                else
                {
                    // Update this edge
                    it->xb(it->xt());

                    it->outp[BELOW] = it->outp[ABOVE];
                    it->outp[ABOVE] = nullptr;

                    it->bstate[BELOW] = it->bstate[ABOVE];

                    it->bundle[BELOW][CLIP] = it->bundle[ABOVE][CLIP];
                    it->bundle[BELOW][SUBJ] = it->bundle[ABOVE][SUBJ];

                    ++it;
                }
            }
        }
    } // END OF SCANBEAM PROCESSING

    // Generate result polygon from out_poly
    if (count_contours(out_poly) > 0)
    {
        for (auto &&poly : out_poly)
        {
            if (poly->vertex_list.is_contributing)
            {
                result.add_contour(poly->proxy->vertex_list.vertexs);
            }
        }
    }
}

void gpc_tristrip_clip(gpc_op op, gpc_polygon *subj, gpc_polygon *clip,
                       gpc_tristrip *result)
{
}

void gpc_polygon_to_tristrip(gpc_polygon *s, gpc_tristrip *t)
{
    gpc_polygon c;
    gpc_tristrip_clip(gpc_op::GPC_DIFF, s, &c, t);
}

} // namespace gpc
