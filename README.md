QOpenGLCacheableShaderProgram is a subclass of QOpenGLShaderProgram adding a few
more functions. By switching the typical

    program->addShaderFromSourceCode(QOpenGLShader::Vertex, ...)
    program->addShaderFromSourceCode(QOpenGLShader::Fragment, ...)

invocations to

    program->addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, ...)
    program->addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, ...)

the compilation may be skipped via gl(Get)ProgramBinary and a disk
cache, when supported. QtGui (paint engine, glyph cache, blitter) and
the Qt Quick scenegraph could all mostly be upgraded to the new
functions with a simple renaming.

Inspired by Qt 5.8's QML/Javascript disk cache, the cache is active whenever the
per-process QStandardPaths::CacheLocation is writable and
QT_DISABLE_SHADER_CACHE is not set.

Why is this needed? In theory it should not add much since some drivers (NVIDIA)
implement caching for a long time, AMD presumably has something similar, while
Mesa has work-in-progress patches.

However, there are still other vendors that do not necessarily have any sort of
cache, and many systems run with older drivers that do not necessarily implement
efficient caching yet.

Some results:

** i.MX6, Linux ** (Vivante GC2000, driver v5.0.11)

10 shaders - standard compilation 192 ms - subsequent runs with Qt's disk cache 41 ms (4.6x speedup)

100 shaders - standard compilation 1634 ms - subsequent runs with Qt's disk cache 95 ms (17.2x speedup)

** AMD Radeon on Windows ** (latest stable Crimson, no driver-provided cache by default?)

10 shaders - standard compilation 118 ms - subsequent runs with Qt's disk cache 16 ms (7.3x speedup)

100 shaders - standard compilation 782 ms - subsequent runs with Qt's disk cache 96 ms (8.1x speedup)

** Intel HD on Windows ** (Surface Pro 3)

10 shaders - standard compilation 59 ms - subsequent runs with Qt's disk cache 13 ms (4.5x speedup)

100 shaders - standard compilation 427 ms - subsequent runs with Qt's disk cache 51 ms (8.3x speedup)

** NVIDIA DRIVE CX ** (driver 367.00, built-in disk cache)

10 shaders - first compilation 8 ms - subsequent runs with driver's cache 8 ms - subsequent runs with Qt's disk cache on top 5 ms (1.6x speedup)

100 shaders - first compilation 693 ms - subsequent runs with driver's cache 51 ms - subsequent runs with Qt's disk cache on top 27 ms (1.8x speedup (25.6x with __GL_SHADER_DISK_CACHE=0))

** Mesa **

Not supported. Even though GL_ARB_get_program_binary is advertised / OpenGL ES
3.0 is supported, the number of supported binary formats is 0. So no speedup
here.
