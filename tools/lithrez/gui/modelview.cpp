// The model preview. An .abc, or an .ltb read with lithtech::ltb (or a PS2
// one with lithtech::ps2ltb) and turned into the same structures (to_abc), is skinned on the CPU (abc::pose and
// skin, as the engine places vertices) and drawn with OpenGL 3.3 core, which
// every platform LithRez Studio runs on offers (macOS only offers core profiles).
//
// Space: LithTech is left-handed with Y up and models face +Z. Negating Z
// gives OpenGL's right-handed space without a mirror image; the negation
// turns the triangle winding round, so corners are sent as 0, 2, 1.
// UVs: DTX rows are stored top row first and LithTech's v points down from
// the top, so with the rows uploaded as stored (u, v) is used as it is.

#include "modelview.h"

#include <lithtech/abc.hpp>
#include <lithtech/abc6.hpp>
#include <lithtech/dtx.hpp>
#include <lithtech/ltb.hpp>
#include <lithtech/ps2ltb.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVersionFunctionsFactory>
#include <QOpenGLWidget>
#include <QTextBrowser>
#include <QUrl>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QSurfaceFormat>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVector3D>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <set>

namespace abc = lithtech::abc;
namespace abc6 = lithtech::abc6;
namespace dtx = lithtech::dtx;
namespace ltb = lithtech::ltb;
namespace ps2ltb = lithtech::ps2ltb;

namespace {

const char* kVertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
uniform mat4 uMVP;
uniform mat4 uMV;
out vec3 vNormal;
out vec2 vUV;
void main() {
    vNormal = mat3(uMV) * aNormal;
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char* kFragmentShader = R"(#version 330 core
in vec3 vNormal;
in vec2 vUV;
uniform sampler2D uTex;
uniform bool uUseTex;
uniform bool uUseAlpha;
uniform bool uTransparentPass;
uniform bool uFlat;
uniform vec3 uColor;
out vec4 fragColor;
void main() {
    vec4 texel = uUseTex ? texture(uTex, vUV) : vec4(uColor, 1.0);
    float alpha = uUseTex && uUseAlpha ? texel.a : 1.0;
    if (alpha <= 0.0 || uTransparentPass != (alpha < 1.0)) discard;
    vec3 base = texel.rgb;
    if (uFlat) { fragColor = vec4(base, 1.0); return; }
    float len = length(vNormal);
    // A headlight: the light comes from the camera. Two-sided, since many
    // pieces are single sheets seen from both sides.
    float diff = len > 0.0 ? abs(normalize(vNormal).z) : 1.0;
    fragColor = vec4(base * (0.35 + 0.65 * diff), alpha);
}
)";

QVector3D toGL(const abc::Vec3& v) { return QVector3D(v[0], v[1], -v[2]); }

// One piece's draw data for the current LOD: per triangle corner.
struct GpuPiece {
    GLuint vao = 0, vbo = 0;
    GLuint texture = 0;      // shared, owned by the texture map
    int corners = 0;
    std::vector<QVector3D> triangleCenters;
};

class GLView : public QOpenGLWidget {
public:
    explicit GLView(QWidget* parent) : QOpenGLWidget(parent) {
        QSurfaceFormat f;
        f.setVersion(3, 3);
        f.setProfile(QSurfaceFormat::CoreProfile);
        f.setDepthBufferSize(24);
        f.setSamples(4);
        setFormat(f);
        setFocusPolicy(Qt::StrongFocus);
        setMinimumSize(200, 200);
    }
    ~GLView() override { releaseGL(); }

    std::function<void(const QString&)> onGLError;

    // --- the model ---
    const abc::Model* model = nullptr;
    std::vector<QImage> skins;           // by texture index; null for none
    int lod = 0;
    bool wireframe = false, skeleton = false, textured = true, transparency = true;
    std::vector<abc::Mat4> matrices;     // current pose, model space
    // The pose's animation and time, for vertex animation (.abc v6).
    const abc::Animation* animation = nullptr;
    float animTime = 0;

    QVector3D center;
    float radius = 1;
    float yaw = 180, pitch = 10, distance = 3;
    QVector3D pan;

    void setModel(const abc::Model* m, std::vector<QImage> sk) {
        model = m;
        skins = std::move(sk);
        texturesDirty = true;
        geometryDirty = true;
        lod = 0;
        animation = nullptr;
        animTime = 0;
        if (m) {
            matrices = abc::bind_pose(*m);
            frame();
        }
        update();
    }

    void frame() {
        if (!model) return;
        QVector3D lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
        bool any = false;
        for (auto& p : model->pieces) {
            if (p.lods.empty()) continue;
            for (auto& v : p.lods[0].vertices) {
                QVector3D q = toGL(abc::skin(v, matrices));
                lo = QVector3D(std::min(lo.x(), q.x()), std::min(lo.y(), q.y()), std::min(lo.z(), q.z()));
                hi = QVector3D(std::max(hi.x(), q.x()), std::max(hi.y(), q.y()), std::max(hi.z(), q.z()));
                any = true;
            }
        }
        if (!any) {
            for (auto& m : matrices) {
                QVector3D q(m[3], m[7], -m[11]);
                lo = QVector3D(std::min(lo.x(), q.x()), std::min(lo.y(), q.y()), std::min(lo.z(), q.z()));
                hi = QVector3D(std::max(hi.x(), q.x()), std::max(hi.y(), q.y()), std::max(hi.z(), q.z()));
                any = true;
            }
        }
        if (!any) { lo = QVector3D(-1, -1, -1); hi = QVector3D(1, 1, 1); }
        center = (lo + hi) * 0.5f;
        radius = std::max(0.5f * (hi - lo).length(), 1e-3f);
        // LithTech models face +Z, which is -Z here: look at the front.
        yaw = 180;
        pitch = 10;
        pan = QVector3D();
        distance = radius / std::tan(qDegreesToRadians(22.5f)) * 1.1f;
        update();
    }

    void poseChanged() {
        geometryDirty = true;
        update();
    }

    QMatrix4x4 viewMatrix() const {
        float y = qDegreesToRadians(yaw), p = qDegreesToRadians(pitch);
        // yaw 0 puts the camera on +Z looking down -Z; 180 on -Z.
        QVector3D dir(std::sin(y) * std::cos(p), std::sin(p), std::cos(y) * std::cos(p));
        QVector3D target = center + pan;
        QMatrix4x4 v;
        v.lookAt(target + dir * distance, target, QVector3D(0, 1, 0));
        return v;
    }

protected:
    void initializeGL() override {
        gl = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(context());
        auto fmt = context()->format();
        if (!gl || fmt.majorVersion() * 10 + fmt.minorVersion() < 33) {
            gl = nullptr;
            if (onGLError)
                onGLError(QStringLiteral("The model preview needs OpenGL 3.3; this system offers %1.%2.")
                              .arg(fmt.majorVersion())
                              .arg(fmt.minorVersion()));
            return;
        }
        prog = std::make_unique<QOpenGLShaderProgram>();
        if (!prog->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader) ||
            !prog->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader) || !prog->link()) {
            if (onGLError) onGLError(QStringLiteral("Couldn't build the preview's shaders:\n") + prog->log());
            gl = nullptr;
            return;
        }
        gl->glGenVertexArrays(1, &lineVao);
        gl->glGenBuffers(1, &lineVbo);
        connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [this] { releaseGL(); });
    }

    void resizeGL(int, int) override {}

    void paintGL() override {
        if (!gl) return;
        gl->glClearColor(0.22f, 0.23f, 0.26f, 1);
        gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!model) return;
        if (texturesDirty) uploadTextures();
        if (geometryDirty) uploadGeometry();

        gl->glEnable(GL_DEPTH_TEST);
        gl->glDisable(GL_CULL_FACE);
        QMatrix4x4 proj;
        float aspect = height() > 0 ? float(width()) / float(height()) : 1.f;
        proj.perspective(45.f, aspect, std::max(distance * 0.01f, 1e-4f), distance * 100.f + radius * 4.f);
        QMatrix4x4 view = viewMatrix();
        prog->bind();
        prog->setUniformValue("uMVP", proj * view);
        prog->setUniformValue("uMV", view);
        prog->setUniformValue("uTex", 0);
        prog->setUniformValue("uFlat", false);
        prog->setUniformValue("uTransparentPass", false);
        gl->glDisable(GL_BLEND);
        gl->glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
        struct TransparentTriangle { const GpuPiece* piece; int first; float depth; };
        std::vector<TransparentTriangle> transparent;
        for (auto& gp : gpu) {
            if (!gp.corners) continue;
            bool useTex = textured && gp.texture;
            prog->setUniformValue("uUseTex", useTex);
            bool useAlpha = useTex && transparency && alphaTextures.count(gp.texture) != 0;
            prog->setUniformValue("uUseAlpha", useAlpha);
            prog->setUniformValue("uColor", QVector3D(0.7f, 0.7f, 0.7f));
            gl->glActiveTexture(GL_TEXTURE0);
            gl->glBindTexture(GL_TEXTURE_2D, useTex ? gp.texture : 0);
            gl->glBindVertexArray(gp.vao);
            gl->glDrawArrays(GL_TRIANGLES, 0, gp.corners);
            if (useAlpha)
                for (std::size_t i = 0; i < gp.triangleCenters.size(); ++i)
                    transparent.push_back({&gp, int(i * 3), view.map(gp.triangleCenters[i]).z()});
        }
        if (!transparent.empty()) {
            // Opaque texels already wrote depth. Blend the rest back to front,
            // including triangles within the same piece (e.g. both sides of glass).
            std::stable_sort(transparent.begin(), transparent.end(), [](const auto& a, const auto& b) {
                return a.depth < b.depth;
            });
            prog->setUniformValue("uTransparentPass", true);
            prog->setUniformValue("uUseTex", true);
            prog->setUniformValue("uUseAlpha", true);
            gl->glEnable(GL_BLEND);
            gl->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            gl->glDepthMask(GL_FALSE);
            std::vector<GLint> firsts;
            std::vector<GLsizei> counts;
            for (std::size_t i = 0; i < transparent.size();) {
                const GpuPiece* gp = transparent[i].piece;
                firsts.clear();
                counts.clear();
                do {
                    firsts.push_back(transparent[i].first);
                    counts.push_back(3);
                    ++i;
                } while (i < transparent.size() && transparent[i].piece == gp);
                gl->glBindTexture(GL_TEXTURE_2D, gp->texture);
                gl->glBindVertexArray(gp->vao);
                gl->glMultiDrawArrays(GL_TRIANGLES, firsts.data(), counts.data(), GLsizei(firsts.size()));
            }
            gl->glDepthMask(GL_TRUE);
            gl->glDisable(GL_BLEND);
            prog->setUniformValue("uTransparentPass", false);
        }
        gl->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        if (skeleton && !matrices.empty()) {
            std::vector<float> v;
            for (std::size_t i = 0; i < model->nodes.size() && i < matrices.size(); ++i) {
                int p = model->nodes[i].parent;
                if (p < 0) continue;
                auto& a = matrices[std::size_t(p)];
                auto& b = matrices[i];
                float seg[12] = {a[3], a[7], -a[11], 0, 0, 0, b[3], b[7], -b[11], 0, 0, 0};
                v.insert(v.end(), seg, seg + 12);
            }
            if (!v.empty()) {
                gl->glDisable(GL_DEPTH_TEST);
                prog->setUniformValue("uUseTex", false);
                prog->setUniformValue("uFlat", true);
                prog->setUniformValue("uColor", QVector3D(1.f, 0.8f, 0.1f));
                gl->glBindVertexArray(lineVao);
                gl->glBindBuffer(GL_ARRAY_BUFFER, lineVbo);
                gl->glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(v.size() * sizeof(float)), v.data(), GL_STREAM_DRAW);
                gl->glEnableVertexAttribArray(0);
                gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
                gl->glEnableVertexAttribArray(1);
                gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                                          reinterpret_cast<void*>(3 * sizeof(float)));
                gl->glDisableVertexAttribArray(2);
                gl->glDrawArrays(GL_LINES, 0, GLsizei(v.size() / 6));
                gl->glEnable(GL_DEPTH_TEST);
            }
        }
        gl->glBindVertexArray(0);
        prog->release();
    }

    void mousePressEvent(QMouseEvent* e) override { last = e->position(); }
    void mouseMoveEvent(QMouseEvent* e) override {
        QPointF dlt = e->position() - last;
        last = e->position();
        bool panning = (e->buttons() & Qt::MiddleButton) ||
                       ((e->buttons() & Qt::LeftButton) && (e->modifiers() & Qt::ShiftModifier));
        if (panning) {
            QMatrix4x4 v = viewMatrix();
            QVector3D right(v(0, 0), v(0, 1), v(0, 2)), up(v(1, 0), v(1, 1), v(1, 2));
            float k = distance * 0.0015f;
            pan += (-right * float(dlt.x()) + up * float(dlt.y())) * k;
        } else if (e->buttons() & Qt::LeftButton) {
            yaw -= float(dlt.x()) * 0.5f;
            pitch = std::clamp(pitch + float(dlt.y()) * 0.5f, -89.f, 89.f);
        } else {
            return;
        }
        update();
    }
    void wheelEvent(QWheelEvent* e) override {
        float steps = float(e->angleDelta().y()) / 120.f;
        distance = std::clamp(distance * std::pow(0.85f, steps), radius * 0.05f, radius * 100.f);
        update();
    }

private:
    QOpenGLFunctions_3_3_Core* gl = nullptr;
    std::unique_ptr<QOpenGLShaderProgram> prog;
    std::vector<GpuPiece> gpu;
    std::map<int, GLuint> textures;
    std::set<GLuint> alphaTextures;
    GLuint lineVao = 0, lineVbo = 0;
    bool texturesDirty = false, geometryDirty = false;
    QPointF last;

    void releaseGL() {
        if (!gl || !context()) return;
        makeCurrent();
        freeGeometry();
        for (auto& [k, t] : textures) gl->glDeleteTextures(1, &t);
        textures.clear();
        if (lineVbo) gl->glDeleteBuffers(1, &lineVbo);
        if (lineVao) gl->glDeleteVertexArrays(1, &lineVao);
        lineVbo = lineVao = 0;
        prog.reset();
        doneCurrent();
        gl = nullptr;
    }

    void freeGeometry() {
        for (auto& g : gpu) {
            if (g.vbo) gl->glDeleteBuffers(1, &g.vbo);
            if (g.vao) gl->glDeleteVertexArrays(1, &g.vao);
        }
        gpu.clear();
    }

    void uploadTextures() {
        texturesDirty = false;
        for (auto& [k, t] : textures) gl->glDeleteTextures(1, &t);
        textures.clear();
        alphaTextures.clear();
        for (std::size_t i = 0; i < skins.size(); ++i) {
            const QImage& img = skins[i];
            if (img.isNull()) continue;
            QImage rgba = img.convertToFormat(QImage::Format_RGBA8888);
            GLuint t = 0;
            gl->glGenTextures(1, &t);
            // Some skins leave alpha entirely zero because it isn't opacity.
            bool nonzeroAlpha = false, nonopaqueAlpha = false;
            for (int y = 0; y < rgba.height(); ++y)
                for (int x = 0; x < rgba.width(); ++x) {
                    uchar a = rgba.constScanLine(y)[x * 4 + 3];
                    nonzeroAlpha |= a != 0;
                    nonopaqueAlpha |= a != 255;
                }
            if (nonzeroAlpha && nonopaqueAlpha) alphaTextures.insert(t);
            gl->glBindTexture(GL_TEXTURE_2D, t);
            gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            // Rows go up as stored, top row first (see the top of the file).
            gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgba.width(), rgba.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                             rgba.constBits());
            gl->glGenerateMipmap(GL_TEXTURE_2D);
            gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            textures[int(i)] = t;
        }
    }

    void uploadGeometry() {
        geometryDirty = false;
        if (gpu.size() != model->pieces.size()) {
            freeGeometry();
            gpu.resize(model->pieces.size());
            for (auto& g : gpu) {
                gl->glGenVertexArrays(1, &g.vao);
                gl->glGenBuffers(1, &g.vbo);
            }
        }
        // Each node's skinning matrix for normals: pose * inverse(bind).
        std::vector<QMatrix4x4> nrm(model->nodes.size());
        for (std::size_t i = 0; i < model->nodes.size() && i < matrices.size(); ++i) {
            QMatrix4x4 P(matrices[i].data()), B(model->nodes[i].bind.data());
            nrm[i] = P * B.inverted();
        }
        std::vector<float> buf;
        for (std::size_t pi = 0; pi < model->pieces.size(); ++pi) {
            auto& piece = model->pieces[pi];
            auto& g = gpu[pi];
            auto it = textures.find(piece.texture_index);
            g.texture = it == textures.end() ? 0 : it->second;
            g.corners = 0;
            g.triangleCenters.clear();
            if (piece.lods.empty()) continue;
            const abc::LOD* shown = &piece.lods[std::size_t(std::clamp(lod, 0, int(piece.lods.size()) - 1))];
            // A v6 model's vertex animation keys vertices of pieces[0]'s LOD 0.
            abc::LOD animated;
            if (animation && pi == 0 && shown == &piece.lods[0] && abc::has_vertex_animation(*model)) {
                animated = *shown;
                abc::animate(*model, *animation, animTime, animated);
                shown = &animated;
            }
            const abc::LOD& L = *shown;
            std::vector<QVector3D> pos(L.vertices.size()), nor(L.vertices.size());
            for (std::size_t vi = 0; vi < L.vertices.size(); ++vi) {
                auto& v = L.vertices[vi];
                pos[vi] = toGL(abc::skin(v, matrices));
                QVector3D n(0, 0, 0), n0(v.normal[0], v.normal[1], v.normal[2]);
                float total = 0;   // biases needn't add up to 1, as abc::skin says
                for (auto& w : v.weights)
                    if (w.node < nrm.size()) n += w.bias * nrm[w.node].mapVector(n0), total += w.bias;
                if (total > 0) n /= total;
                if (v.weights.empty()) n = n0;
                nor[vi] = QVector3D(n.x(), n.y(), -n.z());
            }
            buf.clear();
            buf.reserve(L.triangles.size() * 3 * 8);
            for (auto& t : L.triangles) {
                bool ok = true;
                for (int c = 0; c < 3; ++c)
                    if (t.indices[c] >= L.vertices.size()) ok = false;
                if (!ok) continue;
                g.triangleCenters.push_back((pos[t.indices[0]] + pos[t.indices[1]] + pos[t.indices[2]]) / 3.f);
                for (int c : {0, 2, 1}) {
                    const QVector3D& p = pos[t.indices[c]];
                    const QVector3D& n = nor[t.indices[c]];
                    float corner[8] = {p.x(), p.y(), p.z(), n.x(), n.y(), n.z(), t.uv[c][0], t.uv[c][1]};
                    buf.insert(buf.end(), corner, corner + 8);
                }
            }
            g.corners = int(buf.size() / 8);
            gl->glBindVertexArray(g.vao);
            gl->glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
            gl->glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(buf.size() * sizeof(float)), buf.data(), GL_DYNAMIC_DRAW);
            gl->glEnableVertexAttribArray(0);
            gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
            gl->glEnableVertexAttribArray(1);
            gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                                      reinterpret_cast<void*>(3 * sizeof(float)));
            gl->glEnableVertexAttribArray(2);
            gl->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                                      reinterpret_cast<void*>(6 * sizeof(float)));
        }
        gl->glBindVertexArray(0);
    }
};

QString infoText(const abc::Model& m, const QString& name, const QString& format, int skinsFound, int skinSlots) {
    std::size_t tris = 0, verts = 0;
    for (auto& p : m.pieces)
        if (!p.lods.empty()) {
            tris += p.lods[0].triangles.size();
            verts += p.lods[0].vertices.size();
        }
    QString s;
    s += QStringLiteral("%1\n\n").arg(name);
    s += QStringLiteral("Format       %1\n").arg(format);
    s += QStringLiteral("Pieces       %1\n").arg(m.pieces.size());
    s += QStringLiteral("Nodes        %1\n").arg(m.nodes.size());
    s += QStringLiteral("Triangles    %1 (LOD 0)\n").arg(tris);
    s += QStringLiteral("Vertices     %1 (LOD 0)\n").arg(verts);
    s += QStringLiteral("LODs         %1\n").arg(m.lod_distances.size() + 1);
    s += QStringLiteral("Animations   %1\n").arg(m.animations.size());
    s += QStringLiteral("Sockets      %1\n").arg(m.sockets.size());
    s += QStringLiteral("Skins        %1 of %2 found\n").arg(skinsFound).arg(skinSlots);
    s += QStringLiteral("Radius       %1\n").arg(double(m.internal_radius));
    if (!m.child_models.empty()) {
        s += QStringLiteral("\nChild models (their animations aren't shown):\n");
        for (auto& c : m.child_models) s += QStringLiteral("  %1\n").arg(QString::fromStdString(c));
    }
    if (!m.command_string.empty())
        s += QStringLiteral("\nCommand string:\n  %1\n").arg(QString::fromStdString(m.command_string));
    s += QStringLiteral("\nPieces:\n");
    for (auto& p : m.pieces)
        s += QStringLiteral("  %1  (skin %2, %3 tris)\n")
                 .arg(QString::fromStdString(p.name))
                 .arg(p.texture_index)
                 .arg(p.lods.empty() ? 0 : p.lods[0].triangles.size());
    if (!m.sockets.empty()) {
        s += QStringLiteral("\nSockets:\n");
        for (auto& k : m.sockets) {
            QString node = k.node < m.nodes.size() ? QString::fromStdString(m.nodes[k.node].name) : QStringLiteral("?");
            s += QStringLiteral("  %1  (on %2)\n").arg(QString::fromStdString(k.name), node);
        }
    }
    return s;
}

}  // namespace

struct ModelView::Impl {
    QStackedWidget* stack = nullptr;
    QLabel* message = nullptr;
    GLView* gl = nullptr;
    QTextBrowser* info = nullptr;
    QComboBox* anims = nullptr;
    QToolButton* play = nullptr;
    QSlider* slider = nullptr;
    QLabel* time = nullptr;
    QCheckBox *loop = nullptr, *wire = nullptr, *skel = nullptr, *tex = nullptr, *alpha = nullptr;
    QComboBox* lod = nullptr;
    QTimer timer;
    QElapsedTimer clock;
    float t = 0;
    bool glFailed = false;
    std::unique_ptr<abc::Model> model;

    const abc::Animation* anim() const {
        int i = anims->currentIndex() - 1;
        if (!model || i < 0 || i >= int(model->animations.size())) return nullptr;
        return &model->animations[std::size_t(i)];
    }

    void applyPose() {
        if (!model) return;
        const abc::Animation* a = anim();
        gl->matrices = a ? abc::pose(*model, *a, t) : abc::bind_pose(*model);
        gl->animation = a;
        gl->animTime = t;
        gl->poseChanged();
        time->setText(QStringLiteral("%1 / %2 ms").arg(int(t)).arg(a ? int(a->length_ms()) : 0));
        QSignalBlocker b(slider);
        slider->setValue(int(t));
    }

    void stop() {
        timer.stop();
        play->setText(QStringLiteral("▶"));
    }

    void showMessage(const QString& m) {
        message->setText(m);
        stack->setCurrentIndex(1);
    }
};

ModelView::ModelView(QWidget* parent) : QWidget(parent), d(new Impl) {
    auto* top = new QVBoxLayout(this);
    top->setContentsMargins(0, 0, 0, 0);

    auto* bar = new QHBoxLayout();
    d->anims = new QComboBox(this);
    d->anims->setMinimumContentsLength(14);
    d->anims->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    d->anims->setToolTip(tr("Animation"));
    d->play = new QToolButton(this);
    d->play->setText(QStringLiteral("▶"));
    d->play->setToolTip(tr("Play / pause"));
    d->slider = new QSlider(Qt::Horizontal, this);
    d->time = new QLabel(this);
    d->time->setMinimumWidth(110);
    d->loop = new QCheckBox(tr("Loop"), this);
    d->loop->setChecked(true);
    bar->addWidget(d->anims);
    bar->addWidget(d->play);
    bar->addWidget(d->slider, 1);
    bar->addWidget(d->time);
    bar->addWidget(d->loop);
    top->addLayout(bar);

    auto* bar2 = new QHBoxLayout();
    d->lod = new QComboBox(this);
    d->lod->setToolTip(tr("Level of detail"));
    d->tex = new QCheckBox(tr("Textures"), this);
    d->tex->setChecked(true);
    d->alpha = new QCheckBox(tr("Transparency"), this);
    d->alpha->setChecked(true);
    d->alpha->setToolTip(tr("Blend texture alpha; disable for skins that use alpha as a material mask"));
    d->wire = new QCheckBox(tr("Wireframe"), this);
    d->skel = new QCheckBox(tr("Skeleton"), this);
    auto* reset = new QPushButton(tr("Reset view"), this);
    bar2->addWidget(new QLabel(tr("LOD"), this));
    bar2->addWidget(d->lod);
    bar2->addWidget(d->tex);
    bar2->addWidget(d->alpha);
    bar2->addWidget(d->wire);
    bar2->addWidget(d->skel);
    bar2->addStretch(1);
    bar2->addWidget(reset);
    top->addLayout(bar2);

    d->stack = new QStackedWidget(this);
    auto* split = new QSplitter(Qt::Horizontal, d->stack);
    d->gl = new GLView(split);
    d->info = new QTextBrowser(split);
    d->info->setOpenLinks(false);
    connect(d->info, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
        emit skinActivated(url.path());
    }, Qt::QueuedConnection);
    d->info->setFont(QFont(QStringLiteral("monospace")));
    split->addWidget(d->gl);
    split->addWidget(d->info);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    d->message = new QLabel(d->stack);
    d->message->setAlignment(Qt::AlignCenter);
    d->message->setWordWrap(true);
    d->stack->addWidget(split);
    d->stack->addWidget(d->message);
    top->addWidget(d->stack, 1);

    d->gl->onGLError = [this](const QString& m) {
        d->glFailed = true;
        d->showMessage(m);
    };

    connect(d->anims, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        d->stop();
        d->t = 0;
        const abc::Animation* a = d->anim();
        d->slider->setRange(0, a ? int(a->length_ms()) : 0);
        d->play->setEnabled(a && a->length_ms() > 0);
        d->applyPose();
    });
    connect(d->slider, &QSlider::valueChanged, this, [this](int v) {
        d->t = float(v);
        d->applyPose();
    });
    connect(d->play, &QToolButton::clicked, this, [this] {
        if (d->timer.isActive()) {
            d->stop();
            return;
        }
        const abc::Animation* a = d->anim();
        if (!a || !a->length_ms()) return;
        if (d->t >= float(a->length_ms())) d->t = 0;
        d->clock.restart();
        d->timer.start(16);
        d->play->setText(QStringLiteral("❚❚"));
    });
    connect(&d->timer, &QTimer::timeout, this, [this] {
        const abc::Animation* a = d->anim();
        if (!a || !a->length_ms()) return d->stop();
        d->t += float(d->clock.restart());
        float len = float(a->length_ms());
        if (d->t > len) {
            if (d->loop->isChecked()) {
                d->t = std::fmod(d->t, len);
            } else {
                d->t = len;
                d->stop();
            }
        }
        d->applyPose();
    });
    connect(d->lod, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int i) {
        d->gl->lod = std::max(i, 0);
        d->gl->poseChanged();
    });
    connect(d->tex, &QCheckBox::toggled, this, [this](bool on) {
        d->gl->textured = on;
        d->gl->update();
    });
    connect(d->alpha, &QCheckBox::toggled, this, [this](bool on) {
        d->gl->transparency = on;
        d->gl->update();
    });
    connect(d->wire, &QCheckBox::toggled, this, [this](bool on) {
        d->gl->wireframe = on;
        d->gl->update();
    });
    connect(d->skel, &QCheckBox::toggled, this, [this](bool on) {
        d->gl->skeleton = on;
        d->gl->update();
    });
    connect(reset, &QPushButton::clicked, this, [this] { d->gl->frame(); });

    clear();
}

ModelView::~ModelView() {
    d->timer.stop();
    delete d;
}

bool ModelView::loadAbc(const QByteArray& data, const QString& name, SkinLookup skins, QString* error) {
    clear();
    std::unique_ptr<abc::Model> m;
    QString format;
    try {
        std::vector<uint8_t> bytes(data.begin(), data.end());
        if (ps2ltb::is_ps2_model(bytes)) {
            // a PS2 NOLF model (file type 2), drawn the same way
            ps2ltb::Model pm = ps2ltb::read(bytes);
            format = QStringLiteral("PS2 LTB, version 16");
            m = std::make_unique<abc::Model>(ps2ltb::to_abc(pm));
        } else if (ltb::is_ltb(bytes)) {
            // A Jupiter .ltb, drawn through the same structures as an .abc.
            ltb::Model lm = ltb::read(bytes);
            format = QStringLiteral("LTB, model file version %1").arg(lm.version);
            m = std::make_unique<abc::Model>(ltb::to_abc(lm));
        } else if (abc6::is_model(bytes)) {
            // LithTech 1's .abc (Shogo, Blood 2, KISS), drawn the same way.
            m = std::make_unique<abc::Model>(abc6::to_abc(abc6::read(bytes)));
            format = QStringLiteral("ABC version 6 (LithTech 1)");
        } else {
            m = std::make_unique<abc::Model>(abc::read(bytes));
            format = QStringLiteral("ABC version %1").arg(m->version);
        }
    } catch (const std::exception& e) {
        QString msg = QStringLiteral("Can't show %1: %2").arg(name, QString::fromUtf8(e.what()));
        if (error) *error = msg;
        d->showMessage(msg);
        return false;
    }

    // Skins, by texture slot.
    std::set<int> slotSet;
    for (auto& p : m->pieces) slotSet.insert(p.texture_index);
    int maxSlot = slotSet.empty() ? -1 : *slotSet.rbegin();
    std::vector<QImage> images(std::size_t(maxSlot + 1));
    int found = 0;
    QString skinDetails;
    if (skins)
        for (int s : slotSet) {
            QString path;
            QByteArray b = skins(s, int(slotSet.size()), path);
            if (b.isEmpty()) continue;
            try {
                auto t = dtx::read(std::vector<uint8_t>(b.begin(), b.end()));
                auto img = t.decode(0);
                QImage q(img.width, img.height, QImage::Format_RGBA8888);
                for (int y = 0; y < img.height; ++y)
                    std::memcpy(q.scanLine(y), img.rgba.data() + std::size_t(y) * img.width * 4,
                                std::size_t(img.width) * 4);
                images[std::size_t(s)] = q;
                QUrl url;
                url.setScheme("skin");
                url.setPath(path);
                skinDetails += QStringLiteral("  Skin %1: <a href=\"%2\">%3</a>\n")
                                   .arg(s).arg(url.toString(QUrl::FullyEncoded).toHtmlEscaped(), path.toHtmlEscaped());
                ++found;
            } catch (const std::exception&) {
                // an unreadable skin just leaves the piece grey
            }
        }

    d->model = std::move(m);
    {
        QSignalBlocker b1(d->anims), b2(d->lod);
        d->anims->clear();
        d->anims->addItem(tr("(bind pose)"));
        for (auto& a : d->model->animations) d->anims->addItem(QString::fromStdString(a.name));
        d->lod->clear();
        for (std::size_t i = 0; i <= d->model->lod_distances.size(); ++i)
            d->lod->addItem(i == 0 ? tr("0 (full)")
                                   : QStringLiteral("%1 (%2)").arg(i).arg(double(d->model->lod_distances[i - 1])));
    }
    QString info = infoText(*d->model, name, format, found, int(slotSet.size())).toHtmlEscaped();
    if (!skinDetails.isEmpty()) info += QStringLiteral("\nSkin paths:\n") + skinDetails;
    d->info->setHtml(QStringLiteral("<pre>%1</pre>").arg(info));
    d->gl->setModel(d->model.get(), std::move(images));
    // Start on the first animation's first key if there is one (often the
    // base pose), else the bind pose.
    d->anims->setCurrentIndex(d->model->animations.empty() ? 0 : 1);
    d->slider->setRange(0, d->anim() ? int(d->anim()->length_ms()) : 0);
    d->play->setEnabled(d->anim() && d->anim()->length_ms() > 0);
    d->t = 0;
    d->applyPose();
    d->gl->frame();
    if (!d->glFailed) d->stack->setCurrentIndex(0);
    for (QWidget* w : std::initializer_list<QWidget*>{d->anims, d->slider, d->lod, d->tex, d->alpha, d->wire, d->skel})
        w->setEnabled(true);
    return true;
}

bool ModelView::loadLtb(const QByteArray& data, const QString& name, SkinLookup skins, QString* error) {
    return loadAbc(data, name, std::move(skins), error);   // loadAbc tells the formats apart
}

void ModelView::clear() {
    d->stop();
    d->gl->setModel(nullptr, {});
    d->model.reset();
    d->t = 0;
    {
        QSignalBlocker b1(d->anims), b2(d->lod), b3(d->slider);
        d->anims->clear();
        d->lod->clear();
        d->slider->setRange(0, 0);
    }
    d->info->clear();
    d->time->clear();
    for (QWidget* w : std::initializer_list<QWidget*>{d->anims, d->play, d->slider, d->lod, d->tex, d->alpha, d->wire, d->skel})
        w->setEnabled(false);
    if (!d->glFailed) d->stack->setCurrentIndex(0);
}

void ModelView::setPose(int animation, float timeMs) {
    if (!d->model) return;
    d->stop();
    {
        QSignalBlocker b(d->anims);
        d->anims->setCurrentIndex(animation < 0 ? 0 : std::min(animation + 1, d->anims->count() - 1));
    }
    const abc::Animation* a = d->anim();
    d->slider->setRange(0, a ? int(a->length_ms()) : 0);
    d->t = timeMs;
    d->applyPose();
}

void ModelView::setViewAngles(float yawDegrees, float pitchDegrees) {
    d->gl->yaw = yawDegrees;
    d->gl->pitch = pitchDegrees;
    d->gl->update();
}

QImage ModelView::grabFrame() { return d->gl->grabFramebuffer(); }
