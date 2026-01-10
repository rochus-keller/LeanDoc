#ifndef LEANDOC_TYPST_GEN_H
#define LEANDOC_TYPST_GEN_H

#include <QtCore/QString>
#include <QtCore/QTextStream>
#include <QtCore/QFile>

#include "LeanDocAst2.h"

namespace LeanDoc2 {

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

