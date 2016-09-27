/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qopenglcacheableshaderprogram.h"
#include "qopenglprogrambinarycache_p.h"
#include <QFile>
#include <QLoggingCategory>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QOpenGLExtraFunctions>
#include <QOpenGLContext>

QT_BEGIN_NAMESPACE

#ifndef GL_NUM_PROGRAM_BINARY_FORMATS
#define GL_NUM_PROGRAM_BINARY_FORMATS     0x87FE
#endif

Q_LOGGING_CATEGORY(DBG_SHADER_CACHE, "qt.opengl.diskcache")

struct QOpenGLProgramBinarySupportCheck
{
    QOpenGLProgramBinarySupportCheck();
    bool isSupported() const { return m_supported; }

private:
    bool m_supported;
};

QOpenGLProgramBinarySupportCheck::QOpenGLProgramBinarySupportCheck()
    : m_supported(false)
{
    if (qEnvironmentVariableIntValue("QT_DISABLE_SHADER_CACHE") == 0) {
        QOpenGLContext *ctx = QOpenGLContext::currentContext();
        if (ctx) {
            if (ctx->isOpenGLES()) {
                qCDebug(DBG_SHADER_CACHE, "OpenGL ES v%d context", ctx->format().majorVersion());
                if (ctx->format().majorVersion() >= 3)
                    m_supported = true;
            } else {
                const bool hasExt = ctx->hasExtension("GL_ARB_get_program_binary");
                qCDebug(DBG_SHADER_CACHE, "GL_ARB_get_program_binary support = %d", hasExt);
                if (hasExt)
                    m_supported = true;
            }
            if (m_supported) {
                GLint fmtCount = 0;
                ctx->functions()->glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &fmtCount);
                qCDebug(DBG_SHADER_CACHE, "Supported binary format count = %d", fmtCount);
                m_supported = fmtCount > 0;
            }
        }
        qCDebug(DBG_SHADER_CACHE, "Shader cache supported = %d", m_supported);
    } else {
        qCDebug(DBG_SHADER_CACHE, "Shader cache disabled via env var");
    }
}

Q_GLOBAL_STATIC(QOpenGLProgramBinaryCache, qt_gl_program_binary_cache)
Q_GLOBAL_STATIC(QOpenGLProgramBinarySupportCheck, qt_gl_program_binary_support_check)
    
struct QOpenGLCacheableShaderProgramPrivate
{
    QOpenGLCacheableShaderProgramPrivate(QOpenGLCacheableShaderProgram *q) : q(q) { }

    QOpenGLCacheableShaderProgram *q;
    QOpenGLProgramBinaryCache::ProgramDesc program;

    bool isCacheDisabled() { return !qt_gl_program_binary_support_check()->isSupported(); }

    bool compileCacheable(const QByteArray &cacheKey);
};

QOpenGLCacheableShaderProgram::QOpenGLCacheableShaderProgram(QObject *parent)
    : QOpenGLShaderProgram(parent),
      d(new QOpenGLCacheableShaderProgramPrivate(this))
{
}

QOpenGLCacheableShaderProgram::~QOpenGLCacheableShaderProgram()
{
    delete d;
}

bool QOpenGLCacheableShaderProgram::addCacheableShaderFromSourceCode(QOpenGLShader::ShaderType type, const char *source)
{
    if (d->isCacheDisabled())
        return addShaderFromSourceCode(type, source);

    addCacheableShaderFromSourceCode(type, QByteArray(source));
    return true;
}

bool QOpenGLCacheableShaderProgram::addCacheableShaderFromSourceCode(QOpenGLShader::ShaderType type, const QByteArray &source)
{
    if (d->isCacheDisabled())
        return addShaderFromSourceCode(type, source);

    QOpenGLProgramBinaryCache::ShaderDesc shader;
    shader.type = type;
    shader.source = source;
    d->program.shaders.append(shader);
    return true;
}

bool QOpenGLCacheableShaderProgram::addCacheableShaderFromSourceCode(QOpenGLShader::ShaderType type, const QString &source)
{
    if (d->isCacheDisabled())
        return addShaderFromSourceCode(type, source);

    addCacheableShaderFromSourceCode(type, source.toUtf8().constData());
    return true;
}

bool QOpenGLCacheableShaderProgram::addCacheableShaderFromSourceFile(QOpenGLShader::ShaderType type, const QString &fileName)
{
    if (d->isCacheDisabled())
        return addShaderFromSourceFile(type, fileName);

    QOpenGLProgramBinaryCache::ShaderDesc shader;
    shader.type = type;
    QFile f(fileName);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        shader.source = f.readAll();
        f.close();
    } else {
        qWarning("QOpenGLCacheableShaderProgram: Unable to open file %s", qPrintable(fileName));
        return false;
    }
    d->program.shaders.append(shader);
    return true;
}

bool QOpenGLCacheableShaderProgram::link()
{
    qCDebug(DBG_SHADER_CACHE, "link() program %u", programId());
    if (!d->program.shaders.isEmpty()) {
        QCryptographicHash keyBuilder(QCryptographicHash::Sha1);
        for (const QOpenGLProgramBinaryCache::ShaderDesc &shader : qAsConst(d->program.shaders))
            keyBuilder.addData(shader.source);
        const QByteArray cacheKey = keyBuilder.result().toHex();
        if (DBG_SHADER_CACHE().isEnabled(QtDebugMsg))
            qCDebug(DBG_SHADER_CACHE, "program with %d shaders, cache key %s",
                    d->program.shaders.count(), cacheKey.constData());
        if (qt_gl_program_binary_cache()->load(cacheKey, programId())) {
            qCDebug(DBG_SHADER_CACHE, "Program binary received from cache");
            if (!QOpenGLShaderProgram::link()) {
                qCDebug(DBG_SHADER_CACHE, "Link failed after glProgramBinary; compiling from scratch");
                if (!d->compileCacheable(cacheKey))
                    return false;
            }
        } else {
            qCDebug(DBG_SHADER_CACHE, "Program binary not in cache, compiling");
            if (!d->compileCacheable(cacheKey))
                return false;
        }
    } else {
        qCDebug(DBG_SHADER_CACHE, "Not a binary-based program");
    }

    return QOpenGLShaderProgram::link();
}

bool QOpenGLCacheableShaderProgramPrivate::compileCacheable(const QByteArray &cacheKey)
{
    for (const QOpenGLProgramBinaryCache::ShaderDesc &shader : qAsConst(program.shaders)) {
        QOpenGLShader *s = new QOpenGLShader(shader.type, q);
        if (!s->compileSourceCode(shader.source)) {
            qWarning() << s->log();
            // ### update base d->log
            return false;
        }
        q->addShader(s);
    }
    qt_gl_program_binary_cache()->save(cacheKey, q->programId());
    return true;
}

QT_END_NAMESPACE
