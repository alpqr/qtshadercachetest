#include <QGuiApplication>
#include <QOpenGLWindow>
#include <QPainter>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include "qopenglcacheableshaderprogram.h"

static const char *vsrc =
    "attribute highp vec4 posAttr;\n"
    "attribute lowp vec4 colAttr;\n"
    "varying lowp vec4 col;\n"
    "uniform highp mat4 matrix;\n"
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
        delete m_program;
    }

    QOpenGLCacheableShaderProgram *m_program = nullptr;
    int m_posAttr, m_colAttr, m_matrixUniform;
    QOpenGLBuffer m_vbo;

    void initializeGL() {
        m_program = new QOpenGLCacheableShaderProgram;

        m_program->addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, vsrc);
        m_program->addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);

        if (!m_program->link())
            qFatal("link failed");

        m_posAttr = m_program->attributeLocation("posAttr");
        m_colAttr = m_program->attributeLocation("colAttr");
        m_matrixUniform = m_program->uniformLocation("matrix");

        m_vbo.create();
        m_vbo.bind();
        m_vbo.allocate(vertices, sizeof(vertices) + sizeof(colors));
        m_vbo.write(sizeof(vertices), colors, sizeof(colors));
        m_vbo.release();
    }

    void paintGL() override {
        QOpenGLFunctions *f = context()->functions();
        f->glClear(GL_COLOR_BUFFER_BIT);
        QPainter p(this);
        p.setPen(Qt::red);
        p.drawText(50, 50, "Hello World");
        p.end();

        m_program->bind();
        QMatrix4x4 matrix;
        matrix.perspective(60.0f, 4.0f / 3.0f, 0.1f, 100.0f);
        matrix.translate(0.0f, 0.0f, -2.0f);
        matrix.rotate(20.0f, 0.0f, 1.0f, 0.0f);
        m_program->setUniformValue(m_matrixUniform, matrix);

        m_vbo.bind();
        m_program->setAttributeBuffer(m_posAttr, GL_FLOAT, 0, 2);
        m_program->setAttributeBuffer(m_colAttr, GL_FLOAT, sizeof(vertices), 3);
        m_program->enableAttributeArray(m_posAttr);
        m_program->enableAttributeArray(m_colAttr);
        m_vbo.release();

        f->glDrawArrays(GL_TRIANGLES, 0, 3);
    }
};

int main(int argc, char **argv)
{
    qputenv("QT_LOGGING_RULES", "qt.opengl.*=true");

    // QSurfaceFormat fmt;
    // fmt.setVersion(4, 1);
    // fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
    // QSurfaceFormat::setDefaultFormat(fmt);

    QGuiApplication app(argc, argv);
    Window w;
    w.resize(1024, 768);
    w.show();
    return app.exec();
}
