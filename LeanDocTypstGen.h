#ifndef LEANDOC_TYPST_GEN_H
#define LEANDOC_TYPST_GEN_H

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
#include <QtCore/QTextStream>
#include <QtCore/QFile>

#include "LeanDocAst2.h"

namespace LeanDoc {

struct TypstGenError {
    int line;
    QString message;
    TypstGenError() : line(0) {}
};

class TypstGenerator {
public:
    struct Options {
        QString templateName;     // e.g. "plain", "report"
        QString templateFile;     // optional: import external typst file
        bool allowRawPassthrough; // passthrough blocks/inlines
        Options() : templateName("plain"), allowRawPassthrough(true) {}
    };

    explicit TypstGenerator(const Options& opt) : dopt(opt) {}

    bool generate(const Node* doc, QTextStream& out, TypstGenError* err);

private:
    // top-level
    bool emitPreamble(const Node* doc, QTextStream& out, TypstGenError* err);
    bool emitNode(const Node* n, QTextStream& out, TypstGenError* err, int headingShift);

    // blocks
    bool emitSection(const Node* n, QTextStream& out, TypstGenError* err, int headingShift);
    bool emitParagraph(const Node* n, QTextStream& out, TypstGenError* err);
    bool emitLiteral(const Node* n, QTextStream& out, TypstGenError* err);
    bool emitAdmonition(const Node* n, QTextStream& out, TypstGenError* err);
    bool emitDelimited(const Node* n, QTextStream& out, TypstGenError* err);
    bool emitList(const Node* n, QTextStream& out, TypstGenError* err);
    bool emitTable(const Node* n, QTextStream& out, TypstGenError* err);
    bool emitBlockMacro(const Node* n, QTextStream& out, TypstGenError* err);
    bool emitDirective(const Node* n, QTextStream& out, TypstGenError* err);

    // inline
    bool emitInlineSeq(const QList<Node*>& inl, QTextStream& out, TypstGenError* err);
    bool emitInline(const Node* n, QTextStream& out, TypstGenError* err);

    // helpers
    static QString escText(const QString& s);      // escape plain text in typst markup context
    static QString escString(const QString& s);    // escape for "..." string literals
    static QString labelSuffix(const BlockMeta* m);
    static QString headingMarks(int level);

    Options dopt;
};

} // namespace LeanDoc2

#endif

