// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>

class IPdfRenderer;
class IPdfDocument;
class IPdfWriter;

class BackendRouter {
public:
    static IPdfRenderer* rendererFor(const QString &path);
    static IPdfDocument* documentBackendFor(const QString &path);
    static IPdfWriter* writerFor(const QString &path);
};
