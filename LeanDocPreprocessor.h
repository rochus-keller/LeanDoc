#ifndef LEANDOC_PREPROCESSOR_H
#define LEANDOC_PREPROCESSOR_H

/*
* Copyright 2026 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the LeanDoc document language project.
*
* The following is the license that applies to this copy of the
* file. For a license to use the file under conditions
* other than those described here, please email to me@rochus-keller.ch.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include "LeanDocAst2.h"

namespace LeanDoc {

struct PreprocessorError {
    int line;
    QString message;
    PreprocessorError():line(0){}
    PreprocessorError(int l, const QString& m):line(l),message(m){}
};

class Preprocessor {
public:
    Preprocessor():dmaxIncludeDepth(8){}

    void setBaseDir(const QString& dir) { dbaseDir = dir; }
    void setDefinedAttrs(const QMap<QString,QString>& attrs) { dattrs = attrs; }

    bool process(Node* doc);

    QList<PreprocessorError> errors;

private:
    void collectDocAttrs(Node* doc);
    void processChildren(Node* parent, int depth);
    void processNode(Node* n, int depth);

    bool resolveInclude(Node* parent, int childIdx, int depth);
    bool evaluateConditional(Node* parent, int childIdx);
    void substituteAttrRefs(Node* n);
    void substituteInlineList(QList<Node*>& inl);

    QString readFile(const QString& path);
    QStringList filterByTag(const QStringList& lines, const QString& tag);
    QStringList filterByLines(const QStringList& lines, const QString& spec);

    void error(int line, const QString& msg);

    QString dbaseDir;
    QMap<QString,QString> dattrs;
    QSet<QString> dincludeStack; // circular include detection
    int dmaxIncludeDepth;
};

} // namespace LeanDoc

#endif
