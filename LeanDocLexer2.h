#ifndef LEANDOC_LEXER2_H
#define LEANDOC_LEXER2_H

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
#include <QtCore/QList>

namespace LeanDoc {

struct LineTok {
    enum Kind {
        T_EOF,
        T_BLANK,

        // metadata lines
        T_BLOCK_ANCHOR,    // [[id,...]]
        T_BLOCK_ATTRS,     // [....]
        T_BLOCK_TITLE,     // .Title

        // blocks
        T_SECTION,         // =..====== ...
        T_ADMONITION,      // NOTE: ...
        T_LINE_COMMENT,    // //...

        // breaks
        T_THEMATIC,        // ''', ---, ***
        T_PAGEBREAK,       // <<< ...

        // lists
        T_UL_ITEM,         // *..****** ...
        T_OL_ITEM,         // . .. ... ...
        T_DESC_TERM,       // term:: (COLON{2,})
        T_LIST_CONT,       // +

        // delimited
        T_DELIM_LISTING,       // ----
        T_DELIM_LITERAL,       // ....
        T_DELIM_QUOTE,         // ____
        T_DELIM_EXAMPLE,       // ====
        T_DELIM_SIDEBAR,       // ****
        T_DELIM_OPEN,          // --
        T_DELIM_COMMENT,       // ////

        // tables
        T_TABLE_DELIM,     // |===
        T_TABLE_LINE,      // any line starting with '|'

        // block macros & directives
        T_BLOCK_MACRO,     // image::, include::, ident::...
        T_DIRECTIVE,       // ifdef::, ifndef::, endif::

        // otherwise
        T_TEXT
    };
    static const char* kindName(Kind k);
    const char* kindName() const { return kindName(kind); }

    Kind kind;
    int lineNo;
    QString raw;      // line without trailing newline
    int level;        // for section/list markers (1..6)
    QString head;     // e.g., directive keyword, macro name, admonition label
    QString rest;     // remainder
    LineTok():kind(T_EOF),lineNo(0),level(0){}
};

class Lexer {
public:
    Lexer():dpos(0){}
    void setInput(const QString& text);

    const LineTok& peek(int k=0) const;
    LineTok take();
    bool atEnd() const;

private:
    static bool isBlank(const QString& s);
    static QString trimLeft(const QString& s);
    static bool startsWithRun(const QString& s, QChar ch, int minN, int maxN, int* outN);
    static bool isOnly(const QString& s, const QString& lit);

    static LineTok classify(const QString& line, int lineNo);

private:
    QList<LineTok> dtoks;
    int dpos;
};

} // namespace LeanDoc2

#endif

