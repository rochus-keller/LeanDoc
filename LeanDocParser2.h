#ifndef LEANDOC_PARSER2_H
#define LEANDOC_PARSER2_H

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
#include "LeanDocAst2.h"
#include "LeanDocLexer2.h"

namespace LeanDoc {

class Parser {
public:
    Node* parse(const QString& input);

    struct Error {
        RowCol pos;
        QString message;
        Error(){}
        Error(const QString& m, int l, int c=1):pos(l,c),message(m){}
    };
    QList<Error> errors;

private:
    Node* parseDocument();
    void parseDocumentHeader(Node* doc);

    Node* parseBlock();
    BlockMeta* parseBlockMetaOpt();
    Node* parseSection(BlockMeta* m);
    Node* parseParagraphOrLiteral(BlockMeta* m);
    Node* parseAdmonitionParagraph(BlockMeta* m);
    Node* parseDelimited(BlockMeta* m);
    Node* parseList(BlockMeta* m);
    Node* parseTable(BlockMeta* m);
    Node* parseBlockMacro(BlockMeta* m);
    Node* parseDirective(BlockMeta* m);
    Node* parseBreakOrComment(BlockMeta* m);

    QList<Node*> parseInlineContent(const QString& s, int lineNo);
    QList<Node*> parseInlineContentRec(const QString& s, int lineNo, int depth);
    void pushText(QList<Node*>& out, const QString& t, int lineNo);
    QList<Node*> readCells(const LineTok& rowTok);

    const LineTok& la(int k=0) const { return dlex.peek(k); }
    LineTok take() { return dlex.take(); }
    bool accept(LineTok::Kind k);
    bool expect(LineTok::Kind k, const char* where);
    void error(const QString& msg, int row, int col = 1);
    void skipBlankLines();

    static QMap<QString, QString> parseAttrList(const QString& bracketed);
    static QString stripOuter(const QString& s, QChar a, QChar b);

    Lexer dlex;
};

} // namespace LeanDoc

#endif
