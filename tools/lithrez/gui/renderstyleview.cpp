#include "renderstyleview.h"

#include <lithtech/renderstyle.hpp>

#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace rs = lithtech::renderstyle;

namespace {

QString q(const std::string& s) { return QString::fromStdString(s); }
QString f(float v) { return QString::number(double(v), 'g', 6); }

QIcon swatch(float r, float g, float b, float a = 1) {
    auto c = [](float v) { return int(std::lround(std::clamp(double(v), 0.0, 1.0) * 255)); };
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.fillRect(0, 0, 14, 14, Qt::white);
    p.fillRect(0, 0, 7, 7, Qt::lightGray);
    p.fillRect(7, 7, 7, 7, Qt::lightGray);
    p.fillRect(0, 0, 14, 14, QColor(c(r), c(g), c(b), c(a)));
    p.setPen(Qt::gray);
    p.drawRect(0, 0, 13, 13);
    return QIcon(pm);
}

QTreeWidgetItem* add(QTreeWidgetItem* parent, const QString& k, const QString& v = QString()) {
    return new QTreeWidgetItem(parent, {k, v});
}

void addColour(QTreeWidgetItem* parent, const QString& k, const rs::Colour& c) {
    auto* it = add(parent, k, QStringLiteral("%1, %2, %3, %4").arg(f(c.r), f(c.g), f(c.b), f(c.a)));
    it->setIcon(1, swatch(c.r, c.g, c.b, c.a));
}

QString reg(int32_t r) { return r < 0 ? QStringLiteral("—") : QStringLiteral("c%1").arg(r); }

QString bools(const std::array<bool, 4>& b) {
    QStringList s;
    for (int i = 0; i < 4; ++i)
        if (b[size_t(i)]) s << QString::number(i);
    return s.isEmpty() ? QStringLiteral("—") : QObject::tr("streams %1").arg(s.join(QStringLiteral(", ")));
}

}  // namespace

RenderStyleView::RenderStyleView(QWidget* parent) : QWidget(parent) {
    auto* l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    title_ = new QLabel(this);
    title_->setWordWrap(true);
    title_->setContentsMargins(6, 4, 6, 4);
    tree_ = new QTreeWidget(this);
    tree_->setHeaderLabels({tr("Property"), tr("Value")});
    tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tree_->setUniformRowHeights(true);
    l->addWidget(title_);
    l->addWidget(tree_, 1);
}

void RenderStyleView::clear() {
    title_->clear();
    tree_->clear();
}

bool RenderStyleView::loadRenderStyle(const QByteArray& data, const QString& name, QString* error) {
    clear();
    rs::File file;
    try {
        file = rs::read(reinterpret_cast<const uint8_t*>(data.constData()), size_t(data.size()));
    } catch (const std::exception& e) {
        if (error) *error = tr("Can't read %1: %2").arg(name, QString::fromUtf8(e.what()));
        return false;
    }
    title_->setText(tr("<b>%1</b> &nbsp;render style: %n style(s)", nullptr, int(file.styles.size())).arg(name.toHtmlEscaped()) +
                    (file.styles.size() > 1 ? tr(" (the engine takes the first one the device supports)") : QString()));
    auto* root = tree_->invisibleRootItem();
    auto* hdr = add(root, tr("Header"));
    add(hdr, tr("LTB file type"), tr("%1 (D3D render style)").arg(file.type));
    add(hdr, tr("Version"), QString::number(file.version));
    add(hdr, tr("Total size"), QString::number(file.total_size));
    add(hdr, tr("Reserved"), QStringLiteral("%1, %2, %3, %4").arg(file.reserved1).arg(file.reserved[0]).arg(file.reserved[1]).arg(file.reserved[2]))
        ->setToolTip(1, tr("Not used: whatever was on the packer's stack"));
    if (file.trailing) add(hdr, tr("Trailing bytes"), QString::number(file.trailing));

    for (std::size_t si = 0; si < file.styles.size(); ++si) {
        const rs::Style& s = file.styles[si];
        auto* st = add(root, tr("Style %1").arg(si), tr("%n pass(es)", nullptr, int(s.passes.size())));
        auto* m = add(st, tr("Lighting material"));
        addColour(m, tr("Ambient"), s.material.ambient);
        addColour(m, tr("Diffuse"), s.material.diffuse);
        addColour(m, tr("Emissive"), s.material.emissive);
        addColour(m, tr("Specular"), s.material.specular);
        add(m, tr("Specular power"), f(s.material.specular_power));
        for (std::size_t pi = 0; pi < s.passes.size(); ++pi) {
            const rs::Pass& p = s.passes[pi];
            auto* pass = add(st, tr("Pass %1").arg(pi), q(rs::blend_name(p.blend)));
            add(pass, tr("Blend"), q(rs::blend_name(p.blend)));
            add(pass, tr("Z buffer"), q(rs::z_buffer_name(p.z_buffer)));
            add(pass, tr("Z test"), q(rs::test_name(p.z_test)));
            add(pass, tr("Cull"), q(rs::cull_name(p.cull)));
            add(pass, tr("Fill"), q(rs::fill_name(p.fill)));
            add(pass, tr("Alpha test"), q(rs::test_name(p.alpha_test)));
            add(pass, tr("Alpha ref"), QString::number(p.alpha_ref));
            auto* tf = add(pass, tr("Texture factor"), QStringLiteral("0x%1").arg(p.texture_factor, 8, 16, QLatin1Char('0')));
            tf->setIcon(1, swatch(((p.texture_factor >> 16) & 255) / 255.0f, ((p.texture_factor >> 8) & 255) / 255.0f,
                                  (p.texture_factor & 255) / 255.0f, (p.texture_factor >> 24) / 255.0f));
            add(pass, tr("Dynamic light"), p.dynamic_light ? tr("yes") : tr("no"));
            if (p.bump_env_map)
                add(pass, tr("Bump env map"), tr("stage %1, scale %2, offset %3").arg(p.bump_env_map_stage).arg(f(p.bump_env_map_scale), f(p.bump_env_map_offset)));
            for (int ti = 0; ti < 4; ++ti) {
                const rs::TextureStage& t = p.stages[size_t(ti)];
                if (t.colour_op == 0) {
                    add(pass, tr("Stage %1").arg(ti), tr("disabled"));
                    continue;
                }
                auto* sg = add(pass, tr("Stage %1").arg(ti),
                               QStringLiteral("%1 (%2, %3)").arg(q(rs::colour_op_name(t.colour_op)), q(rs::colour_arg_name(t.colour_arg1)),
                                                                q(rs::colour_arg_name(t.colour_arg2))));
                add(sg, tr("Texture"), q(rs::texture_param_name(t.texture)));
                auto* co = add(sg, tr("Colour"), QStringLiteral("%1 (%2, %3)").arg(q(rs::colour_op_name(t.colour_op)),
                                                                              q(rs::colour_arg_name(t.colour_arg1)),
                                                                              q(rs::colour_arg_name(t.colour_arg2))));
                if (t.colour_op >= 16)
                    co->setToolTip(1, tr("Past the games' list of colour ops: their lookup table (16 entries) is read past its end"));
                add(sg, tr("Alpha"), QStringLiteral("%1 (%2, %3)").arg(q(rs::alpha_op_name(t.alpha_op)), q(rs::alpha_arg_name(t.alpha_arg1)),
                                                                       q(rs::alpha_arg_name(t.alpha_arg2))));
                add(sg, tr("UV source"), q(rs::uv_source_name(t.uv_source)));
                add(sg, tr("Address"), QStringLiteral("U %1, V %2").arg(q(rs::uv_address_name(t.u_address)), q(rs::uv_address_name(t.v_address))));
                add(sg, tr("Filter"), q(rs::filter_name(t.filter)));
                if (t.uv_transform) {
                    auto* mx = add(sg, tr("UV transform"), tr("on"));
                    for (int r = 0; r < 4; ++r)
                        add(mx, tr("Row %1").arg(r), QStringLiteral("%1  %2  %3  %4").arg(f(t.uv_matrix[size_t(4 * r)]), f(t.uv_matrix[size_t(4 * r + 1)]),
                                                                                       f(t.uv_matrix[size_t(4 * r + 2)]), f(t.uv_matrix[size_t(4 * r + 3)])));
                } else {
                    add(sg, tr("UV transform"), tr("off"));
                }
                add(sg, tr("Projected"), t.project_tex_coord ? tr("yes") : tr("no"));
                add(sg, tr("Coordinate count"), QString::number(t.tex_coord_count));
            }
            if (!p.has_d3d) continue;
            const rs::D3DPass& d = p.d3d;
            auto* dd = add(pass, tr("Direct3D options"), d.use_vertex_shader ? tr("vertex shader (the games skip it)") : tr("no vertex shader"));
            add(dd, tr("Expand for skinning"), d.expand_for_skinning ? tr("yes") : tr("no"));
            for (int i = 0; i < 3; ++i)
                add(dd, tr("Constant %1").arg(i + 1), QStringLiteral("%1 = (%2, %3, %4, %5)").arg(reg(d.const_reg[i]), f(d.const_param[i].x),
                                                                                                 f(d.const_param[i].y), f(d.const_param[i].z), f(d.const_param[i].w)));
            add(dd, tr("World-view"), tr("%1, %2 matrices").arg(reg(d.world_view_reg)).arg(d.world_view_count));
            add(dd, tr("Projection"), reg(d.proj_reg));
            add(dd, tr("World-view-projection"), reg(d.world_view_proj_reg));
            add(dd, tr("View-projection"), reg(d.view_proj_reg));
            add(dd, tr("Camera position"), reg(d.cam_pos_reg));
            add(dd, tr("Lights"), tr("%1: position %2 (model space), %3 (camera space), colour %4, attenuation %5")
                                      .arg(d.light_count).arg(reg(d.light_pos_model_reg), reg(d.light_pos_camera_reg), reg(d.light_colour_reg), reg(d.light_att_reg)));
            add(dd, tr("Material"), tr("ambient/diffuse/emissive %1, specular %2").arg(reg(d.material_amb_dif_em_reg), reg(d.material_specular_reg)));
            add(dd, tr("Ambient light"), reg(d.ambient_light_reg));
            add(dd, tr("Previous world-view"), tr("%1, %2 matrices").arg(reg(d.prev_world_view_reg)).arg(d.prev_world_view_count));
            add(dd, tr("Last constant"), reg(d.last_reg));
            add(dd, tr("Positions"), bools(d.stream_position));
            add(dd, tr("Normals"), bools(d.stream_normal));
            add(dd, tr("UV sets"), bools(d.stream_uvsets) + QStringLiteral("  (%1, %2, %3, %4)").arg(d.stream_uv_count[0]).arg(d.stream_uv_count[1])
                                                                .arg(d.stream_uv_count[2]).arg(d.stream_uv_count[3]));
            add(dd, tr("Basis vectors"), bools(d.stream_basis));
            for (std::size_t b = 0; b < d.blocks.size(); ++b) {
                const auto& blk = d.blocks[b];
                QString what = tr("%n byte(s)", nullptr, int(blk.size()));
                // a D3D8 shader starts with its version token (vs.1.1: 0xfffe0101)
                if (blk.size() >= 4 && blk[2] == 0xfe && blk[3] == 0xff)
                    what += tr(", vs.%1.%2 code").arg(blk[1]).arg(blk[0]);
                add(dd, tr("Shader block %1").arg(b), what);
            }
        }
    }
    tree_->expandAll();
    // the vertex shader's details stay folded
    for (QTreeWidgetItemIterator it(tree_); *it; ++it)
        if ((*it)->text(0) == tr("Direct3D options") || (*it)->text(0) == tr("UV transform") || (*it)->text(0) == tr("Header"))
            (*it)->setExpanded(false);
    return true;
}
