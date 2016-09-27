#include <QGuiApplication>
#include <QOpenGLWindow>
#include <QPainter>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QDateTime>
#include <QElapsedTimer>
#include "qopenglcacheableshaderprogram.h"

static const int COUNT = 100;
bool DIFF = false;

static const char *vsrc =
    "attribute highp vec4 posAttr;\n"
    "attribute lowp vec4 colAttr;\n"
    "varying lowp vec4 col;\n"
    "uniform highp mat4 matrix;\n"
    "//$$\n"
    "void main() {\n"
    "   col = colAttr;\n"
    "   gl_Position = matrix * posAttr;\n"
    "}\n";

static const char *fsrc =
    "varying lowp vec4 col;\n"
    "void main() {\n"
    "   gl_FragColor = col;\n"
    "}\n";

static GLfloat vertices[] = {
    0.0f, 0.707f,
    -0.5f, -0.5f,
    0.5f, -0.5f
};

static GLfloat colors[] = {
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f
};

class Window : public QOpenGLWindow
{
public:
    Window() {
    }
    ~Window() {
        makeCurrent();
        qDeleteAll(m_programs);
    }

    QVector<QOpenGLCacheableShaderProgram *> m_programs;
    int m_posAttr, m_colAttr, m_matrixUniform;
    QOpenGLBuffer m_vbo;
    QElapsedTimer initToFirstFrameTimer;
    bool m_first = true;

    void initializeGL() {
        initToFirstFrameTimer.start();

        qint64 ts = 0;
        if (DIFF) {
            QDateTime dt = QDateTime::currentDateTime();
            ts = dt.toMSecsSinceEpoch();
        }
        for (int i = 0; i < COUNT; ++i) {
            QOpenGLCacheableShaderProgram *prog = new QOpenGLCacheableShaderProgram;
            QByteArray vs(vsrc);
            QString s("uniform highp float f%1;");
            s = s.arg(ts + i);
            vs.replace("//$$", s.toLatin1());
            prog->addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, vs);
            prog->addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);
            if (!prog->link())
                qFatal("link failed");
            m_programs.append(prog);
        }

        QOpenGLCacheableShaderProgram *prog = m_programs[0];
        m_posAttr = prog->attributeLocation("posAttr");
        m_colAttr = prog->attributeLocation("colAttr");
        m_matrixUniform = prog->uniformLocation("matrix");

        m_vbo.create();
        m_vbo.bind();
        m_vbo.allocate(vertices, sizeof(vertices) + sizeof(colors));
        m_vbo.write(sizeof(vertices), colors, sizeof(colors));
        m_vbo.release();
    }

    void paintGL() override {
        QOpenGLFunctions *f = context()->functions();
        f->glClear(GL_COLOR_BUFFER_BIT);
        // QPainter p(this);
        // p.setPen(Qt::red);
        // p.drawText(50, 50, "Hello World");
        // p.end();

        QOpenGLCacheableShaderProgram *prog = m_programs[0];
        prog->bind();
        QMatrix4x4 matrix;
        matrix.perspective(60.0f, 4.0f / 3.0f, 0.1f, 100.0f);
        matrix.translate(0.0f, 0.0f, -2.0f);
        matrix.rotate(20.0f, 0.0f, 1.0f, 0.0f);
        prog->setUniformValue(m_matrixUniform, matrix);

        m_vbo.bind();
        prog->setAttributeBuffer(m_posAttr, GL_FLOAT, 0, 2);
        prog->setAttributeBuffer(m_colAttr, GL_FLOAT, sizeof(vertices), 3);
        prog->enableAttributeArray(m_posAttr);
        prog->enableAttributeArray(m_colAttr);
        m_vbo.release();

        f->glDrawArrays(GL_TRIANGLES, 0, 3);

        if (m_first) {
            m_first = false;
            qDebug("\n\n%lld ms\n\n", initToFirstFrameTimer.elapsed());
        }
    }
};

int main(int argc, char **argv)
{
    //qputenv("QT_LOGGING_RULES", "qt.opengl.*=true");

    // QSurfaceFormat fmt;
    // fmt.setVersion(4, 1);
    // fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
    // QSurfaceFormat::setDefaultFormat(fmt);

    QGuiApplication app(argc, argv);
    const QStringList args = app.arguments();
    for (int i = 0; i < args.count(); ++i)
        if (args[i] == QStringLiteral("--recompile"))
            DIFF = true;

    Window w;
    w.resize(1024, 768);
    w.show();
    return app.exec();
}
