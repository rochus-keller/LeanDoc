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

#include "LeanDocLexer2.h"

namespace LeanDoc {

const char* LineTok::kindName(Kind k)
{
    switch (k) {
    case T_EOF: return "EOF";
    case T_BLANK: return "BLANK";
    case T_BLOCK_ANCHOR: return "BLOCK_ANCHOR";
    case T_BLOCK_ATTRS: return "BLOCK_ATTRS";
    case T_BLOCK_TITLE: return "BLOCK_TITLE";
    case T_SECTION: return "SECTION";
    case T_ADMONITION: return "ADMONITION";
    case T_LINE_COMMENT: return "LINE_COMMENT";
    case T_THEMATIC: return "THEMATIC_BREAK";
    case T_PAGEBREAK: return "PAGE_BREAK";
    case T_UL_ITEM: return "UL_ITEM";
    case T_OL_ITEM: return "OL_ITEM";
    case T_DESC_TERM: return "DESC_TERM";
    case T_LIST_CONT: return "LIST_CONT";
    case T_DELIM_LISTING: return "DELIM_LISTING";
    case T_DELIM_LITERAL: return "DELIM_LITERAL";
    case T_DELIM_QUOTE: return "DELIM_QUOTE";
    case T_DELIM_EXAMPLE: return "DELIM_EXAMPLE";
    case T_DELIM_SIDEBAR: return "DELIM_SIDEBAR";
    case T_DELIM_OPEN: return "DELIM_OPEN";
    case T_DELIM_COMMENT: return "DELIM_COMMENT";
    case T_TABLE_DELIM: return "TABLE_DELIM";
    case T_TABLE_LINE: return "TABLE_LINE";
    case T_BLOCK_MACRO: return "BLOCK_MACRO";
    case T_DIRECTIVE: return "DIRECTIVE";
    case T_TEXT: return "TEXT";
    default: return "UNKNOWN";
    }
}

static bool isPrefix(const QString& s, const QString& p) {
    return s.startsWith(p);
}

void Lexer::setInput(const QString& text)
{
    dtoks.clear();
    dpos = 0;

    QStringList lines = text.split('\n'); // keeps empty trailing last line as empty
    for (int i=0;i<lines.size();++i) {
        dtoks.append(classify(lines[i], i+1));
    }
    LineTok eof;
    eof.kind = LineTok::T_EOF;
    eof.lineNo = lines.size()+1;
    dtoks.append(eof);
}

const LineTok& Lexer::peek(int k) const
{
    int idx = dpos + k;
    if (idx < 0)
        idx = 0;
    if (idx >= dtoks.size())
        idx = dtoks.size()-1;
    return dtoks[idx];
}

LineTok Lexer::take()
{
    LineTok t = peek(0);
    if (dpos < dtoks.size())
        ++dpos;
    return t;
}

bool Lexer::atEnd() const {
    return peek(0).kind == LineTok::T_EOF;
}

bool Lexer::isBlank(const QString& s)
{
    for (int i=0;i<s.size();++i)
        if (!s[i].isSpace())
            return false;
    return true;
}

QString Lexer::trimLeft(const QString& s)
{
    int i=0;
    while (i<s.size() && s[i].isSpace())
        ++i;
    return s.mid(i);
}

bool Lexer::startsWithRun(const QString& s, QChar ch, int minN, int maxN, int* outN)
{
    int n=0;
    while (n<s.size() && n<maxN && s[n]==ch)
        ++n;
    if (n>=minN) {
        if (outN)
            *outN=n;
        return true;
    }
    return false;
}

bool Lexer::isOnly(const QString& s, const QString& lit)
{
    return s.trimmed() == lit;
}

LineTok Lexer::classify(const QString& line, int lineNo)
{
    LineTok t;
    t.lineNo = lineNo;
    t.raw = line;

    const QString s = line.trimmed();
    if (s.isEmpty()) {
        t.kind = LineTok::T_BLANK;
        return t;
    }

    // metadata lines
    if (s.startsWith("[[") && s.endsWith("]]")) {
        t.kind = LineTok::T_BLOCK_ANCHOR;
        t.rest = s;
        return t;
    }
    if (s.size()>=2 && s[0]=='.' && !s[1].isSpace()) { // ".Title" per grammar (no forced space)
        t.kind = LineTok::T_BLOCK_TITLE;
        t.rest = s.mid(1);
        return t;
    }

    // directives (preprocessor)
    if (isPrefix(s, "ifdef::") || isPrefix(s, "ifndef::") || isPrefix(s, "endif::")) {
        t.kind = LineTok::T_DIRECTIVE;
        int p = s.indexOf("::");
        t.head = s.left(p);
        t.rest = s.mid(p+2);
        return t;
    }

    // block macros
    if (isPrefix(s, "include::")) {
        // image:: is now just a normal paragraph which only contains an image: or latexmath:
        t.kind = LineTok::T_BLOCK_MACRO;
        int p = s.indexOf("::");
        t.head = s.left(p);
        t.rest = s.mid(p+2);
        return t;
    }
    // custom block macro: IDENT::target[...]
    {
        int p = s.indexOf("::");
        if (p > 0 && s.indexOf('[') > p) {
            t.kind = LineTok::T_BLOCK_MACRO;
            t.head = s.left(p);
            t.rest = s.mid(p+2);
            return t;
        }
    }

    // comments & breaks
    if (isPrefix(s, "//")) {
        t.kind = LineTok::T_LINE_COMMENT;
        t.rest = s.mid(2);
        return t;
    }
    if (s == "'''" || s == "---" || s == "***") {
        t.kind = LineTok::T_THEMATIC;
        return t;
    }
    if (isPrefix(s, "<<<")) {
        t.kind = LineTok::T_PAGEBREAK;
        t.rest = s.mid(3).trimmed();
        return t;
    }

    // section title =..====== (1..6) then space+
    int eqN=0;
    if (startsWithRun(s, '=', 1, 6, &eqN) && s.size()>eqN && s[eqN].isSpace()) {
        t.kind = LineTok::T_SECTION;
        t.level = eqN;
        t.rest = s.mid(eqN).trimmed();
        return t;
    }

    // lists
    int starN=0;
    if (startsWithRun(s, '*', 1, 6, &starN) && s.size()>starN && s[starN].isSpace()) {
        t.kind = LineTok::T_UL_ITEM;
        t.level = starN;
        t.rest = s.mid(starN).trimmed();
        return t;
    }
    int dotN=0;
    if (startsWithRun(s, '.', 1, 6, &dotN) && s.size()>dotN && s[dotN].isSpace()) {
        t.kind = LineTok::T_OL_ITEM;
        t.level = dotN;
        t.rest = s.mid(dotN).trimmed();
        return t;
    }
    if (s == "+") {
        t.kind = LineTok::T_LIST_CONT;
        return t;
    }

    // description list term: TEXT_RUN then "::" (2+) at end (simplified detection)
    {
        int p = s.lastIndexOf(':');
        if (p >= 1 && s.endsWith("::")) {
            // count trailing colons
            int c = 0;
            for (int i=s.size()-1;i>=0 && s[i]==':';--i)
                ++c;
            if (c >= 2) {
                t.kind = LineTok::T_DESC_TERM;
                t.level = c;
                t.rest = s.left(s.size()-c).trimmed();
                return t;
            }
        }
    }

    // tables
    if (s == "|===") {
        t.kind = LineTok::T_TABLE_DELIM;
        return t;
    }
    if (s.startsWith("|")) {
        t.kind = LineTok::T_TABLE_LINE;
        t.rest = line;
        return t;
    }

    // delimited blocks
    if (s == "----") {
        t.kind = LineTok::T_DELIM_LISTING;
        return t;
    }
    if (s == "....") {
        t.kind = LineTok::T_DELIM_LITERAL;
        return t;
    }
    if (s == "____") {
        t.kind = LineTok::T_DELIM_QUOTE;
        return t;
    }
    if (s == "====") {
        t.kind = LineTok::T_DELIM_EXAMPLE;
        return t;
    }
    if (s == "****") {
        t.kind = LineTok::T_DELIM_SIDEBAR;
        return t;
    }
    if (s == "--") {
        t.kind = LineTok::T_DELIM_OPEN;
        return t;
    }
    if (s == "////") {
        t.kind = LineTok::T_DELIM_COMMENT;
        return t;
    }

    // admonition paragraph NOTE:/TIP:/...
    if (s.startsWith("NOTE:") || s.startsWith("TIP:") || s.startsWith("IMPORTANT:") ||
        s.startsWith("CAUTION:") || s.startsWith("WARNING:")) {
        t.kind = LineTok::T_ADMONITION;
        int c = s.indexOf(':');
        t.head = s.left(c);
        t.rest = s.mid(c+1).trimmed();
        return t;
    }

    t.kind = LineTok::T_TEXT;
    t.rest = line;
    return t;
}

} // namespace LeanDoc2

