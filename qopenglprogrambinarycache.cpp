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

#include "qopenglprogrambinarycache_p.h"
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QStandardPaths>
#include <QDir>
#include <QLoggingCategory>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(DBG_SHADER_CACHE)

#ifndef GL_PROGRAM_BINARY_LENGTH
#define GL_PROGRAM_BINARY_LENGTH          0x8741
#endif

// all of QOpenGLProgramBinaryCache must be thread-safe

const quint32 BINSHADER_MAGIC = 0x9604;
const quint32 BINSHADER_VERSION = 0x1;
const quint32 BINSHADER_QTVERSION = QT_VERSION;

struct BinCacheCommon
{
    BinCacheCommon();

    QByteArray glvendor;
    QByteArray glrenderer;
    QByteArray glversion;
};

BinCacheCommon::BinCacheCommon()
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    Q_ASSERT(ctx);
    QOpenGLFunctions *f = ctx->functions();
    const char *vendor = reinterpret_cast<const char *>(f->glGetString(GL_VENDOR));
    const char *renderer = reinterpret_cast<const char *>(f->glGetString(GL_RENDERER));
    const char *version = reinterpret_cast<const char *>(f->glGetString(GL_VERSION));
    if (vendor)
        glvendor = QByteArray(vendor);
    if (renderer)
        glrenderer = QByteArray(renderer);
    if (version)
        glversion = QByteArray(version);
}

QOpenGLProgramBinaryCache::QOpenGLProgramBinaryCache()
{
    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QLatin1String("/qtshadercache/");
    QDir::root().mkpath(m_cacheDir);
    m_cacheWritable = QFileInfo(m_cacheDir).isWritable();
    qCDebug(DBG_SHADER_CACHE, "Cache location '%s' writable = %d", qPrintable(m_cacheDir), m_cacheWritable);
}

QString QOpenGLProgramBinaryCache::cacheFileName(const QByteArray &cacheKey) const
{
    return m_cacheDir + QString::fromUtf8(cacheKey);
}

bool QOpenGLProgramBinaryCache::load(const QByteArray &cacheKey, uint programId)
{
    QFile f(cacheFileName(cacheKey)); // ### switch to mmap on unix
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QByteArray buf = f.readAll();
    f.close();

    if (buf.size() < 12 + 12 + 8) {
        qCDebug(DBG_SHADER_CACHE, "Cached size too small");
        f.remove();
        return false;
    }
    const quint32 *p = reinterpret_cast<const quint32 *>(buf.constData());
    if (*p++ != BINSHADER_MAGIC) {
        qCDebug(DBG_SHADER_CACHE, "Magic does not match");
        f.remove();
        return false;
    }
    if (*p++ != BINSHADER_VERSION) {
        qCDebug(DBG_SHADER_CACHE, "Version does not match");
        f.remove();
        return false;
    }
    if (*p++ != BINSHADER_QTVERSION) {
        qCDebug(DBG_SHADER_CACHE, "Qt version does not match");
        f.remove();
        return false;
    }

    BinCacheCommon b;

    quint32 v = *p++;
    QByteArray vendor = QByteArray::fromRawData(reinterpret_cast<const char *>(p), v);
    if (vendor != b.glvendor) {
        qCDebug(DBG_SHADER_CACHE, "GL_VENDOR does not match (%s, %s)", qPrintable(vendor), qPrintable(b.glvendor));
        f.remove();
        return false;
    }
    p = reinterpret_cast<const quint32 *>(reinterpret_cast<const char *>(p) + v);
    v = *p++;
    QByteArray renderer = QByteArray::fromRawData(reinterpret_cast<const char *>(p), v);
    if (renderer != b.glrenderer) {
        qCDebug(DBG_SHADER_CACHE, "GL_RENDERER does not match (%s, %s)", qPrintable(renderer), qPrintable(b.glrenderer));
        f.remove();
        return false;
    }
    p = reinterpret_cast<const quint32 *>(reinterpret_cast<const char *>(p) + v);
    v = *p++;
    QByteArray version = QByteArray::fromRawData(reinterpret_cast<const char *>(p), v);
    if (version != b.glversion) {
        qCDebug(DBG_SHADER_CACHE, "GL_VERSION does not match (%s, %s)", qPrintable(version), qPrintable(b.glversion));
        f.remove();
        return false;
    }
    p = reinterpret_cast<const quint32 *>(reinterpret_cast<const char *>(p) + v);

    quint32 blobFormat = *p++;
    quint32 blobSize = *p++;

    QOpenGLExtraFunctions *funcs = QOpenGLContext::currentContext()->extraFunctions();
    funcs->glGetError();
    funcs->glProgramBinary(programId, blobFormat, p, blobSize);
    int err = funcs->glGetError();
    qCDebug(DBG_SHADER_CACHE, "Program binary set for program %u, size %d, format 0x%x, err = 0x%x",
            programId, blobSize, blobFormat, err);

    return err == 0;
}

void QOpenGLProgramBinaryCache::save(const QByteArray &cacheKey, uint programId)
{
    if (!m_cacheWritable)
        return;

    BinCacheCommon b;

    QOpenGLExtraFunctions *funcs = QOpenGLContext::currentContext()->extraFunctions();
    GLint blobSize = 0;
    funcs->glGetError();
    funcs->glGetProgramiv(programId, GL_PROGRAM_BINARY_LENGTH, &blobSize);
    int totalSize = blobSize + 8 + 12 + 12 + b.glvendor.count() + b.glrenderer.count() + b.glversion.count();
    qCDebug(DBG_SHADER_CACHE, "Program binary is %d bytes, err = 0x%x, total %d", blobSize, funcs->glGetError(), totalSize);
    if (!blobSize)
        return;

    QByteArray blob;
    blob.resize(totalSize);
    quint32 *p = reinterpret_cast<quint32 *>(blob.data());

    *p++ = BINSHADER_MAGIC;
    *p++ = BINSHADER_VERSION;
    *p++ = BINSHADER_QTVERSION;

    *p++ = b.glvendor.count();
    memcpy(p, b.glvendor.constData(), b.glvendor.count());
    p = reinterpret_cast<quint32 *>(reinterpret_cast<char *>(p) + b.glvendor.count());
    *p++ = b.glrenderer.count();
    memcpy(p, b.glrenderer.constData(), b.glrenderer.count());
    p = reinterpret_cast<quint32 *>(reinterpret_cast<char *>(p) + b.glrenderer.count());
    *p++ = b.glversion.count();
    memcpy(p, b.glversion.constData(), b.glversion.count());
    p = reinterpret_cast<quint32 *>(reinterpret_cast<char *>(p) + b.glversion.count());
        
    quint32 blobFormat = 0;
    GLint outSize = 0;
    quint32 *fmtP = p++;
    *p++ = blobSize;
    funcs->glGetProgramBinary(programId, blobSize, &outSize, &blobFormat, p);
    if (blobSize != outSize) {
        qCDebug(DBG_SHADER_CACHE, "glGetProgramBinary returned size %d instead of %d", outSize, blobSize);
        return;
    }
    *fmtP = blobFormat;

    QFile f(cacheFileName(cacheKey));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(blob);
    else
        qCDebug(DBG_SHADER_CACHE, "Failed to write %s to shader cache", qPrintable(f.fileName()));
}

QT_END_NAMESPACE
